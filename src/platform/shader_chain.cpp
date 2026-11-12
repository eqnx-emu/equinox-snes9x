#include "platform/shader_chain.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>

#include "shader_source.hpp"

namespace {

enum Kind : int {
    kMvp, kFrameDirection, kFrameCount, kOutputSize, kTextureSize, kInputSize, kOrigInputSize,
    kSampler, kParam
};

// Attribute slots, bound before linking.
constexpr GLuint kVertexCoord = 0, kTexCoord = 1, kColor = 2;

// GL_HALF_FLOAT_OES: the GLES2 half-float type, not in the GLES3 headers.
constexpr GLenum kHalfFloatOes = 0x8D61;

// The final copy to the screen, in the RetroArch format so it draws like any pass.
constexpr const char* kBlitVs =
    "#version 100\n"
    "attribute vec4 VertexCoord; attribute vec4 TexCoord; uniform mat4 MVPMatrix;\n"
    "varying vec2 uv;\n"
    "void main() { uv = TexCoord.xy; gl_Position = MVPMatrix * VertexCoord; }\n";
constexpr const char* kBlitFs =
    "#version 100\nprecision mediump float;\n"
    "uniform sampler2D Texture; varying vec2 uv;\n"
    "void main() { gl_FragColor = texture2D(Texture, uv); }\n";

// Maps the 0..1 quad onto the whole target, column major.
constexpr GLfloat kOrtho[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, -1, -1, 0, 1};
constexpr GLfloat kQuad[8] = {0, 0, 1, 0, 0, 1, 1, 1};

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

GLuint compile(GLenum type, const std::string& src, const std::string& what) {
    const GLuint sh = gl::f.CreateShader(type);
    const char* p = src.c_str();
    gl::f.ShaderSource(sh, 1, &p, nullptr);
    gl::f.CompileShader(sh);
    GLint ok = 0;
    gl::f.GetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        gl::f.GetShaderInfoLog(sh, sizeof log, nullptr, log);
        std::fprintf(stderr, "shader: %s (%s) does not compile: %s\n", what.c_str(),
                     type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        gl::f.DeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint link(const std::string& vs, const std::string& fs, const std::string& what) {
    const GLuint v = compile(GL_VERTEX_SHADER, vs, what);
    if (!v) return 0;
    const GLuint f = compile(GL_FRAGMENT_SHADER, fs, what);
    if (!f) {
        gl::f.DeleteShader(v);
        return 0;
    }
    const GLuint prog = gl::f.CreateProgram();
    gl::f.AttachShader(prog, v);
    gl::f.AttachShader(prog, f);
    gl::f.BindAttribLocation(prog, kVertexCoord, "VertexCoord");
    gl::f.BindAttribLocation(prog, kTexCoord, "TexCoord");
    gl::f.BindAttribLocation(prog, kColor, "COLOR");
    gl::f.LinkProgram(prog);
    gl::f.DeleteShader(v);
    gl::f.DeleteShader(f);
    GLint ok = 0;
    gl::f.GetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        gl::f.GetProgramInfoLog(prog, sizeof log, nullptr, log);
        std::fprintf(stderr, "shader: %s does not link: %s\n", what.c_str(), log);
        gl::f.DeleteProgram(prog);
        return 0;
    }
    return prog;
}

GLint wrapMode(Wrap w) {
    switch (w) {
        case Wrap::Repeat: return GL_REPEAT;
        case Wrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
        case Wrap::ClampToEdge: break;
    }
    return GL_CLAMP_TO_EDGE;
}

}  // namespace

ShaderChain::~ShaderChain() { unload(); }

bool ShaderChain::load(const std::string& presetFile, std::span<const ParamOverride> overrides,
                       const gl::Info& info) {
    unload();
    info_ = info;
    const auto text = readFile(presetFile);
    if (!text) {
        std::fprintf(stderr, "shader: cannot read %s\n", presetFile.c_str());
        return false;
    }
    const size_t slash = presetFile.find_last_of('/');
    const auto preset = parsePreset(*text, slash == std::string::npos ? "." : presetFile.substr(0, slash));
    if (!preset) {
        std::fprintf(stderr, "shader: %s is not a supported preset\n", presetFile.c_str());
        return false;
    }
    preset_ = *preset;

    // Parameter values: the shader's default, then the preset's, then the intensity's.
    std::map<std::string, float> values;
    std::vector<GLuint> programs;
    for (const PassSpec& spec : preset_.passes) {
        const auto src = readFile(spec.shader);
        if (!src) {
            std::fprintf(stderr, "shader: cannot read %s\n", spec.shader.c_str());
            for (GLuint p : programs) gl::f.DeleteProgram(p);
            return false;
        }
        for (const ParamDecl& d : parseParameters(*src)) values.emplace(d.name, d.def);
        const bool gles3 = info.major >= 3;
        const auto vs = buildStage(*src, Stage::Vertex, gles3);
        const auto fs = buildStage(*src, Stage::Fragment, gles3);
        const GLuint prog = vs && fs ? link(*vs, *fs, spec.shader) : 0;
        if (!vs || !fs) std::fprintf(stderr, "shader: %s needs GLES 3\n", spec.shader.c_str());
        if (!prog) {
            for (GLuint p : programs) gl::f.DeleteProgram(p);
            return false;
        }
        programs.push_back(prog);
    }
    for (const auto& [name, value] : preset_.parameters) values[name] = value;
    for (const ParamOverride& o : overrides) values[o.name] = o.value;

    needsBlit_ = needsFinalBlit(preset_);
    if (needsBlit_) {
        blit_.program = link(kBlitVs, kBlitFs, "final copy");
        if (!blit_.program) {
            for (GLuint p : programs) gl::f.DeleteProgram(p);
            return false;
        }
    }

    auto collect = [&](Pass& pass) {
        static const std::map<std::string, int> kBuiltins = {
            {"MVPMatrix", kMvp},       {"FrameDirection", kFrameDirection},
            {"FrameCount", kFrameCount}, {"OutputSize", kOutputSize},
            {"TextureSize", kTextureSize}, {"InputSize", kInputSize},
            {"OrigInputSize", kOrigInputSize}};
        GLint count = 0;
        gl::f.GetProgramiv(pass.program, GL_ACTIVE_UNIFORMS, &count);
        for (GLint i = 0; i < count; ++i) {
            char name[128] = {};
            GLint size = 0;
            GLenum type = 0;
            gl::f.GetActiveUniform(pass.program, static_cast<GLuint>(i), sizeof name, nullptr, &size, &type,
                                   name);
            if (char* bracket = std::strchr(name, '[')) *bracket = '\0';
            const GLint loc = gl::f.GetUniformLocation(pass.program, name);
            if (loc < 0) continue;
            if (type == GL_SAMPLER_2D) {
                pass.uniforms.push_back({loc, type, kSampler, 0});
            } else if (const auto b = kBuiltins.find(name); b != kBuiltins.end()) {
                pass.uniforms.push_back({loc, type, b->second, 0});
            } else if (const auto v = values.find(name); v != values.end()) {
                pass.uniforms.push_back({loc, type, kParam, v->second});
            }
        }
    };
    passes_.resize(programs.size());
    for (size_t i = 0; i < programs.size(); ++i) {
        passes_[i].program = programs[i];
        collect(passes_[i]);
    }
    if (needsBlit_) collect(blit_);
    std::fprintf(stderr, "shader: %s loaded, %zu passes%s\n", presetFile.c_str(), passes_.size(),
                 needsBlit_ ? " and a final copy" : "");
    return true;
}

void ShaderChain::freeTargets() {
    for (Pass& p : passes_) {
        if (p.fbo) gl::f.DeleteFramebuffers(1, &p.fbo);
        if (p.target) gl::f.DeleteTextures(1, &p.target);
        p.fbo = p.target = 0;
    }
    sizes_.clear();
    targetsFor_ = targetsView_ = Size2{0, 0};
}

void ShaderChain::unload() {
    freeTargets();
    for (Pass& p : passes_)
        if (p.program) gl::f.DeleteProgram(p.program);
    passes_.clear();
    if (blit_.program) gl::f.DeleteProgram(blit_.program);
    blit_ = Pass{};
    needsBlit_ = false;
    if (frame_) gl::f.DeleteTextures(1, &frame_);
    frame_ = 0;
    frameSize_ = Size2{0, 0};
    preset_ = Preset{};
}

void ShaderChain::upload(const uint16_t* px565, unsigned w, unsigned h) {
    if (!loaded() || !px565 || w == 0 || h == 0) return;
    GLint align = 4;
    gl::f.GetIntegerv(GL_UNPACK_ALIGNMENT, &align);
    gl::f.PixelStorei(GL_UNPACK_ALIGNMENT, 2);
    gl::f.ActiveTexture(GL_TEXTURE0);
    if (!frame_) gl::f.GenTextures(1, &frame_);
    gl::f.BindTexture(GL_TEXTURE_2D, frame_);
    if (frameSize_.w != w || frameSize_.h != h) {
        frameSize_ = Size2{w, h};
        gl::f.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0, GL_RGB,
                         GL_UNSIGNED_SHORT_5_6_5, px565);
    } else {
        gl::f.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGB,
                            GL_UNSIGNED_SHORT_5_6_5, px565);
    }
    gl::f.PixelStorei(GL_UNPACK_ALIGNMENT, align);
}

// One render target per pass that does not draw on screen, remade when the game frame
// or the viewport changes size (SNES 256/512 widths, 224/239 lines, dock and undock).
bool ShaderChain::ensureTargets(Size2 viewport) {
    if (!sizes_.empty() && targetsFor_.w == frameSize_.w && targetsFor_.h == frameSize_.h &&
        targetsView_.w == viewport.w && targetsView_.h == viewport.h)
        return true;
    freeTargets();
    sizes_ = passSizes(preset_, frameSize_, viewport);
    targetsFor_ = frameSize_;
    targetsView_ = viewport;
    const size_t offscreen = needsBlit_ ? passes_.size() : passes_.size() - 1;
    for (size_t i = 0; i < offscreen; ++i) {
        Pass& p = passes_[i];
        const Size2 s = sizes_[i];
        bool half = preset_.passes[i].floatFramebuffer && info_.halfFloatTarget;
        if (preset_.passes[i].floatFramebuffer && !half && !floatWarned_) {
            std::fprintf(stderr, "shader: no half-float targets, pass %zu uses 8 bits per channel\n", i);
            floatWarned_ = true;
        }
        gl::f.GenTextures(1, &p.target);
        gl::f.GenFramebuffers(1, &p.fbo);
        for (int attempt = 0; attempt < 2; ++attempt) {
            gl::f.BindTexture(GL_TEXTURE_2D, p.target);
            const GLint internal = half ? (info_.major >= 3 ? GL_RGBA16F : GL_RGBA) : GL_RGBA;
            const GLenum type = half ? (info_.major >= 3 ? GL_HALF_FLOAT : kHalfFloatOes) : GL_UNSIGNED_BYTE;
            gl::f.TexImage2D(GL_TEXTURE_2D, 0, internal, static_cast<GLsizei>(s.w), static_cast<GLsizei>(s.h), 0,
                             GL_RGBA, type, nullptr);
            gl::f.BindFramebuffer(GL_FRAMEBUFFER, p.fbo);
            gl::f.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p.target, 0);
            if (gl::f.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) break;
            if (!half) {
                std::fprintf(stderr, "shader: render target %ux%u for pass %zu is incomplete\n", s.w, s.h, i);
                return false;
            }
            std::fprintf(stderr, "shader: half-float target refused, pass %zu uses 8 bits per channel\n", i);
            half = false;
        }
    }
    return true;
}

void ShaderChain::drawPass(const Pass& pass, GLuint input, Size2 inSize, Size2 outSize, bool fromGameFrame,
                           std::optional<bool> filterLinear, Wrap wrap, unsigned frameCount) {
    gl::f.ActiveTexture(GL_TEXTURE0);
    gl::f.BindTexture(GL_TEXTURE_2D, input);
    const GLint filter = filterLinear.value_or(false) ? GL_LINEAR : GL_NEAREST;
    gl::f.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    gl::f.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    gl::f.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode(wrap));
    gl::f.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode(wrap));

    gl::f.UseProgram(pass.program);
    for (const Uniform& u : pass.uniforms) {
        switch (u.kind) {
            case kMvp: gl::f.UniformMatrix4fv(u.location, 1, GL_FALSE, kOrtho); break;
            case kSampler: gl::f.Uniform1i(u.location, 0); break;
            case kOutputSize: gl::f.Uniform2f(u.location, float(outSize.w), float(outSize.h)); break;
            case kTextureSize:
            case kInputSize: gl::f.Uniform2f(u.location, float(inSize.w), float(inSize.h)); break;
            case kOrigInputSize: gl::f.Uniform2f(u.location, float(frameSize_.w), float(frameSize_.h)); break;
            case kFrameDirection:
            case kFrameCount:
            case kParam: {
                const float v = u.kind == kFrameDirection ? 1.0f
                                : u.kind == kFrameCount   ? static_cast<float>(frameCount)
                                                          : u.value;
                if (u.type == GL_INT) gl::f.Uniform1i(u.location, static_cast<GLint>(v));
                else gl::f.Uniform1f(u.location, v);
                break;
            }
        }
    }
    const auto uv = passTexCoords(fromGameFrame);
    gl::f.VertexAttribPointer(kVertexCoord, 2, GL_FLOAT, GL_FALSE, 0, kQuad);
    gl::f.VertexAttribPointer(kTexCoord, 2, GL_FLOAT, GL_FALSE, 0, uv.data());
    gl::f.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

bool ShaderChain::draw(int vx, int vy, int vw, int vh, unsigned frameCount) {
    if (!loaded() || !frame_ || vw <= 0 || vh <= 0) return false;
    GLint screen = 0;
    gl::f.GetIntegerv(GL_FRAMEBUFFER_BINDING, &screen);
    const Size2 viewport{static_cast<unsigned>(vw), static_cast<unsigned>(vh)};
    if (!ensureTargets(viewport)) {
        freeTargets();
        gl::f.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen));
        return false;
    }
    gl::f.BindBuffer(GL_ARRAY_BUFFER, 0);
    for (GLuint i = 0; i < 8; ++i) gl::f.DisableVertexAttribArray(i);
    gl::f.EnableVertexAttribArray(kVertexCoord);
    gl::f.EnableVertexAttribArray(kTexCoord);

    GLuint input = frame_;
    Size2 inSize = frameSize_;
    for (size_t i = 0; i < passes_.size(); ++i) {
        const Pass& p = passes_[i];
        if (p.fbo) {
            gl::f.BindFramebuffer(GL_FRAMEBUFFER, p.fbo);
            gl::f.Viewport(0, 0, static_cast<GLsizei>(sizes_[i].w), static_cast<GLsizei>(sizes_[i].h));
        } else {
            gl::f.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen));
            gl::f.Viewport(vx, vy, vw, vh);
        }
        const PassSpec& spec = preset_.passes[i];
        drawPass(p, input, inSize, sizes_[i], i == 0, spec.filterLinear, spec.wrap, frameCount);
        input = p.target;
        inSize = sizes_[i];
    }
    if (needsBlit_) {
        gl::f.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen));
        gl::f.Viewport(vx, vy, vw, vh);
        drawPass(blit_, input, inSize, viewport, false, true, Wrap::ClampToEdge, frameCount);
    }
    return true;
}

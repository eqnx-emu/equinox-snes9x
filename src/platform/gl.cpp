#include "platform/gl.hpp"

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstring>

namespace gl {

Funcs f;

bool load() {
    bool ok = true;
#define EQX_GL_LOAD(type, name, sym)                                   \
    f.name = reinterpret_cast<type>(SDL_GL_GetProcAddress(sym));       \
    if (!f.name) {                                                     \
        std::fprintf(stderr, "GL entry point %s missing\n", sym);      \
        ok = false;                                                    \
    }
    EQX_GL_FUNCS(EQX_GL_LOAD)
#undef EQX_GL_LOAD
    return ok;
}

namespace {
bool hasExtension(const char* list, const char* name) {
    if (!list) return false;
    const size_t n = std::strlen(name);
    for (const char* p = list; (p = std::strstr(p, name)); p += n)
        if ((p == list || p[-1] == ' ') && (p[n] == ' ' || p[n] == '\0')) return true;
    return false;
}
}  // namespace

Info queryInfo() {
    Info info;
    const char* version = reinterpret_cast<const char*>(f.GetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(f.GetString(GL_RENDERER));
    const char* ext = reinterpret_cast<const char*>(f.GetString(GL_EXTENSIONS));
    if (version) std::sscanf(version, "OpenGL ES %d.%d", &info.major, &info.minor);
    const bool es32 = info.major > 3 || (info.major == 3 && info.minor >= 2);
    info.halfFloatTarget = es32 || hasExtension(ext, "GL_EXT_color_buffer_half_float") ||
                           (info.major >= 3 && hasExtension(ext, "GL_EXT_color_buffer_float"));
    std::fprintf(stderr, "GL: %s | %s | ES %d.%d | half-float targets: %s\n",
                 version ? version : "?", renderer ? renderer : "?", info.major, info.minor,
                 info.halfFloatTarget ? "yes" : "no");
    return info;
}

StateGuard::StateGuard() {
    f.GetIntegerv(GL_CURRENT_PROGRAM, &program_);
    f.GetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer_);
    f.GetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
    f.ActiveTexture(GL_TEXTURE0);
    f.GetIntegerv(GL_TEXTURE_BINDING_2D, &texture0_);
    f.GetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer_);
    f.GetIntegerv(GL_VIEWPORT, viewport_);
    f.GetIntegerv(GL_SCISSOR_BOX, scissorBox_);
    scissor_ = f.IsEnabled(GL_SCISSOR_TEST);
    blend_ = f.IsEnabled(GL_BLEND);
    f.GetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb_);
    f.GetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb_);
    f.GetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha_);
    f.GetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha_);
    for (int i = 0; i < kAttribs; ++i)
        f.GetVertexAttribiv(static_cast<GLuint>(i), GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attribEnabled_[i]);
    // The shader passes draw opaque quads over the whole target.
    f.Disable(GL_SCISSOR_TEST);
    f.Disable(GL_BLEND);
}

StateGuard::~StateGuard() {
    for (int i = 0; i < kAttribs; ++i) {
        if (attribEnabled_[i]) f.EnableVertexAttribArray(static_cast<GLuint>(i));
        else f.DisableVertexAttribArray(static_cast<GLuint>(i));
    }
    f.BlendFuncSeparate(static_cast<GLenum>(blendSrcRgb_), static_cast<GLenum>(blendDstRgb_),
                        static_cast<GLenum>(blendSrcAlpha_), static_cast<GLenum>(blendDstAlpha_));
    if (blend_) f.Enable(GL_BLEND);
    else f.Disable(GL_BLEND);
    f.Scissor(scissorBox_[0], scissorBox_[1], scissorBox_[2], scissorBox_[3]);
    if (scissor_) f.Enable(GL_SCISSOR_TEST);
    else f.Disable(GL_SCISSOR_TEST);
    f.Viewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
    f.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer_));
    f.ActiveTexture(GL_TEXTURE0);
    f.BindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture0_));
    f.ActiveTexture(static_cast<GLenum>(activeTexture_));
    f.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer_));
    f.UseProgram(static_cast<GLuint>(program_));
}

}  // namespace gl

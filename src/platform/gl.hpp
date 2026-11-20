#pragma once

// OpenGL ES entry points for drawing the game through shaders in the middle of the
// SDL renderer's frame. The SDL renderer owns the context; these are loaded from it
// with SDL_GL_GetProcAddress, so they reach the same driver SDL draws with.

#include <GLES2/gl2.h>
#include <GLES3/gl3.h>

#define EQX_GL_FUNCS(X)                                                              \
    X(PFNGLACTIVETEXTUREPROC, ActiveTexture, "glActiveTexture")                      \
    X(PFNGLATTACHSHADERPROC, AttachShader, "glAttachShader")                         \
    X(PFNGLBINDATTRIBLOCATIONPROC, BindAttribLocation, "glBindAttribLocation")       \
    X(PFNGLBINDBUFFERPROC, BindBuffer, "glBindBuffer")                               \
    X(PFNGLBINDFRAMEBUFFERPROC, BindFramebuffer, "glBindFramebuffer")                \
    X(PFNGLBINDTEXTUREPROC, BindTexture, "glBindTexture")                            \
    X(PFNGLBLENDFUNCSEPARATEPROC, BlendFuncSeparate, "glBlendFuncSeparate")          \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, CheckFramebufferStatus, "glCheckFramebufferStatus") \
    X(PFNGLCLEARPROC, Clear, "glClear")                                              \
    X(PFNGLCLEARCOLORPROC, ClearColor, "glClearColor")                               \
    X(PFNGLCOMPILESHADERPROC, CompileShader, "glCompileShader")                      \
    X(PFNGLCREATEPROGRAMPROC, CreateProgram, "glCreateProgram")                      \
    X(PFNGLCREATESHADERPROC, CreateShader, "glCreateShader")                         \
    X(PFNGLDELETEFRAMEBUFFERSPROC, DeleteFramebuffers, "glDeleteFramebuffers")       \
    X(PFNGLDELETEPROGRAMPROC, DeleteProgram, "glDeleteProgram")                      \
    X(PFNGLDELETESHADERPROC, DeleteShader, "glDeleteShader")                         \
    X(PFNGLDELETETEXTURESPROC, DeleteTextures, "glDeleteTextures")                   \
    X(PFNGLDISABLEPROC, Disable, "glDisable")                                        \
    X(PFNGLDISABLEVERTEXATTRIBARRAYPROC, DisableVertexAttribArray, "glDisableVertexAttribArray") \
    X(PFNGLDRAWARRAYSPROC, DrawArrays, "glDrawArrays")                               \
    X(PFNGLENABLEPROC, Enable, "glEnable")                                           \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray, "glEnableVertexAttribArray") \
    X(PFNGLFINISHPROC, Finish, "glFinish")                                           \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, FramebufferTexture2D, "glFramebufferTexture2D") \
    X(PFNGLGENFRAMEBUFFERSPROC, GenFramebuffers, "glGenFramebuffers")                \
    X(PFNGLGENTEXTURESPROC, GenTextures, "glGenTextures")                            \
    X(PFNGLGETACTIVEUNIFORMPROC, GetActiveUniform, "glGetActiveUniform")             \
    X(PFNGLGETBOOLEANVPROC, GetBooleanv, "glGetBooleanv")                            \
    X(PFNGLGETERRORPROC, GetError, "glGetError")                                     \
    X(PFNGLGETINTEGERVPROC, GetIntegerv, "glGetIntegerv")                            \
    X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog, "glGetProgramInfoLog")          \
    X(PFNGLGETPROGRAMIVPROC, GetProgramiv, "glGetProgramiv")                         \
    X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog, "glGetShaderInfoLog")             \
    X(PFNGLGETSHADERIVPROC, GetShaderiv, "glGetShaderiv")                            \
    X(PFNGLGETSTRINGPROC, GetString, "glGetString")                                  \
    X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation, "glGetUniformLocation")       \
    X(PFNGLGETVERTEXATTRIBIVPROC, GetVertexAttribiv, "glGetVertexAttribiv")          \
    X(PFNGLISENABLEDPROC, IsEnabled, "glIsEnabled")                                  \
    X(PFNGLLINKPROGRAMPROC, LinkProgram, "glLinkProgram")                            \
    X(PFNGLPIXELSTOREIPROC, PixelStorei, "glPixelStorei")                            \
    X(PFNGLSCISSORPROC, Scissor, "glScissor")                                        \
    X(PFNGLSHADERSOURCEPROC, ShaderSource, "glShaderSource")                         \
    X(PFNGLTEXIMAGE2DPROC, TexImage2D, "glTexImage2D")                               \
    X(PFNGLTEXPARAMETERIPROC, TexParameteri, "glTexParameteri")                      \
    X(PFNGLTEXSUBIMAGE2DPROC, TexSubImage2D, "glTexSubImage2D")                      \
    X(PFNGLUNIFORM1FPROC, Uniform1f, "glUniform1f")                                  \
    X(PFNGLUNIFORM1IPROC, Uniform1i, "glUniform1i")                                  \
    X(PFNGLUNIFORM2FPROC, Uniform2f, "glUniform2f")                                  \
    X(PFNGLUNIFORMMATRIX4FVPROC, UniformMatrix4fv, "glUniformMatrix4fv")             \
    X(PFNGLUSEPROGRAMPROC, UseProgram, "glUseProgram")                               \
    X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer, "glVertexAttribPointer")    \
    X(PFNGLVIEWPORTPROC, Viewport, "glViewport")

namespace gl {

struct Funcs {
#define EQX_GL_FIELD(type, name, sym) type name = nullptr;
    EQX_GL_FUNCS(EQX_GL_FIELD)
#undef EQX_GL_FIELD
};

extern Funcs f;

// Loads every entry point above from the current SDL GL context. False when one is
// missing; shader filters are then off and the game draws as before.
bool load();

struct Info {
    int major = 2, minor = 0;
    // A half-float color attachment can be rendered to (float_framebuffer passes).
    bool halfFloatTarget = false;
};

// Reads GL_VERSION ("OpenGL ES 3.2 Mesa ...") and the extensions, and logs them.
Info queryInfo();

// Saves the GL state the SDL renderer caches, restores it on destruction: SDL's
// GLES2 renderer assumes nothing else touched the context between its own calls.
class StateGuard {
public:
    StateGuard();
    ~StateGuard();
    StateGuard(const StateGuard&) = delete;
    StateGuard& operator=(const StateGuard&) = delete;

private:
    static constexpr int kAttribs = 8;
    GLint program_ = 0, arrayBuffer_ = 0, activeTexture_ = 0, texture0_ = 0, framebuffer_ = 0;
    GLint viewport_[4] = {}, scissorBox_[4] = {};
    GLboolean scissor_ = GL_FALSE, blend_ = GL_FALSE;
    GLint blendSrcRgb_ = 0, blendDstRgb_ = 0, blendSrcAlpha_ = 0, blendDstAlpha_ = 0;
    GLint attribEnabled_[kAttribs] = {};
};

}  // namespace gl

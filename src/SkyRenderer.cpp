#include "customskies/SkyRenderer.hpp"
#include "customskies/Diag.hpp"

#include <dlfcn.h>
#include <cstring>

namespace customskies {
namespace {

// Minimal GLES2 types / constants
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLfloat = float;
using GLchar = char;

constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_CULL_FACE = 0x0B44;
constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
constexpr GLenum GL_ALWAYS = 0x0207;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_FALSE = 0;
constexpr GLenum GL_TRUE = 1;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_DEPTH_BUFFER_BIT = 0x00000100;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_FRAMEBUFFER_BINDING = 0x8CA6;
constexpr GLenum GL_VIEWPORT = 0x0BA2;
constexpr GLenum GL_NO_ERROR = 0;

struct Gl {
    void (*GenTextures)(GLsizei, GLuint*) = nullptr;
    void (*DeleteTextures)(GLsizei, const GLuint*) = nullptr;
    void (*BindTexture)(GLenum, GLuint) = nullptr;
    void (*TexParameteri)(GLenum, GLenum, GLint) = nullptr;
    void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
    void (*TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) = nullptr;
    void (*PixelStorei)(GLenum, GLint) = nullptr;

    GLuint (*CreateShader)(GLenum) = nullptr;
    void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
    void (*CompileShader)(GLuint) = nullptr;
    void (*GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*DeleteShader)(GLuint) = nullptr;
    GLuint (*CreateProgram)() = nullptr;
    void (*AttachShader)(GLuint, GLuint) = nullptr;
    void (*LinkProgram)(GLuint) = nullptr;
    void (*GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*DeleteProgram)(GLuint) = nullptr;
    void (*UseProgram)(GLuint) = nullptr;
    GLint (*GetAttribLocation)(GLuint, const GLchar*) = nullptr;
    GLint (*GetUniformLocation)(GLuint, const GLchar*) = nullptr;
    void (*Uniform1i)(GLint, GLint) = nullptr;
    void (*Uniform1f)(GLint, GLfloat) = nullptr;

    void (*GenBuffers)(GLsizei, GLuint*) = nullptr;
    void (*DeleteBuffers)(GLsizei, const GLuint*) = nullptr;
    void (*BindBuffer)(GLenum, GLuint) = nullptr;
    void (*BufferData)(GLenum, long, const void*, GLenum) = nullptr;
    void (*EnableVertexAttribArray)(GLuint) = nullptr;
    void (*DisableVertexAttribArray)(GLuint) = nullptr;
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
    void (*DrawArrays)(GLenum, GLint, GLsizei) = nullptr;

    void (*Enable)(GLenum) = nullptr;
    void (*Disable)(GLenum) = nullptr;
    void (*DepthFunc)(GLenum) = nullptr;
    void (*DepthMask)(GLboolean) = nullptr;
    void (*ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean) = nullptr;
    void (*BlendFunc)(GLenum, GLenum) = nullptr;
    void (*GetIntegerv)(GLenum, GLint*) = nullptr;
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    GLenum (*GetError)() = nullptr;
    void (*ActiveTexture)(GLenum) = nullptr;

    bool ok = false;
};

Gl g;

void* loadSym(void* lib, const char* name) {
    void* p = lib ? dlsym(lib, name) : nullptr;
    if (!p) p = dlsym(RTLD_DEFAULT, name);
    return p;
}

bool loadGl() {
    if (g.ok) return true;
    void* gles = dlopen("libGLESv2.so", RTLD_NOW);
    if (!gles) gles = dlopen("libGLESv3.so", RTLD_NOW);

#define L(name) g.name = reinterpret_cast<decltype(g.name)>(loadSym(gles, "gl" #name))
    L(GenTextures); L(DeleteTextures); L(BindTexture); L(TexParameteri);
    L(TexImage2D); L(TexSubImage2D); L(PixelStorei);
    L(CreateShader); L(ShaderSource); L(CompileShader); L(GetShaderiv); L(DeleteShader);
    L(CreateProgram); L(AttachShader); L(LinkProgram); L(GetProgramiv); L(DeleteProgram);
    L(UseProgram); L(GetAttribLocation); L(GetUniformLocation); L(Uniform1i); L(Uniform1f);
    L(GenBuffers); L(DeleteBuffers); L(BindBuffer); L(BufferData);
    L(EnableVertexAttribArray); L(DisableVertexAttribArray); L(VertexAttribPointer);
    L(DrawArrays); L(Enable); L(Disable); L(DepthFunc); L(DepthMask); L(ColorMask);
    L(BlendFunc); L(GetIntegerv); L(Viewport); L(GetError); L(ActiveTexture);
#undef L

    g.ok = g.CreateProgram && g.DrawArrays && g.TexImage2D && g.GenTextures;
    if (!g.ok) diag::error("SkyRenderer: failed to resolve GLES symbols");
    return g.ok;
}

const char* kVert = R"(
attribute vec2 aPos;
varying vec2 vUv;
void main() {
    vUv = vec2(aPos.x * 0.5 + 0.5, 1.0 - (aPos.y * 0.5 + 0.5));
    gl_Position = vec4(aPos, 1.0, 1.0);
}
)";

const char* kFrag = R"(
precision mediump float;
uniform sampler2D uTex;
uniform float uOpacity;
varying vec2 vUv;
void main() {
    vec4 c = texture2D(uTex, vUv);
    gl_FragColor = vec4(c.rgb, uOpacity);
}
)";

GLuint compile(GLenum type, const char* src) {
    GLuint s = g.CreateShader(type);
    g.ShaderSource(s, 1, &src, nullptr);
    g.CompileShader(s);
    GLint ok = 0;
    g.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        diag::error("SkyRenderer: shader compile failed");
        g.DeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

SkyRenderer::SkyRenderer() = default;

SkyRenderer::~SkyRenderer() {
    shutdown();
}

bool SkyRenderer::init() {
    if (mReady) return true;
    if (!loadGl()) return false;
    if (!buildProgram()) return false;

    g.GenBuffers(1, &mVbo);
    // Fullscreen triangle strip as 2 triangles (6 verts): clip-space
    const float verts[] = {
        -1.f, -1.f,
         1.f, -1.f,
        -1.f,  1.f,
        -1.f,  1.f,
         1.f, -1.f,
         1.f,  1.f,
    };
    g.BindBuffer(GL_ARRAY_BUFFER, mVbo);
    g.BufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    g.BindBuffer(GL_ARRAY_BUFFER, 0);

    g.GenTextures(1, &mTex);
    g.BindTexture(GL_TEXTURE_2D, mTex);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g.BindTexture(GL_TEXTURE_2D, 0);

    mReady = true;
    diag::info("SkyRenderer: init OK");
    return true;
}

void SkyRenderer::shutdown() {
    if (!g.ok) {
        mReady = false;
        return;
    }
    if (mTex) { g.DeleteTextures(1, &mTex); mTex = 0; }
    if (mVbo) { g.DeleteBuffers(1, &mVbo); mVbo = 0; }
    if (mProg) { g.DeleteProgram(mProg); mProg = 0; }
    mTexW = mTexH = 0;
    mUploadedSerial = 0;
    mReady = false;
}

bool SkyRenderer::buildProgram() {
    GLuint vs = compile(GL_VERTEX_SHADER, kVert);
    GLuint fs = compile(GL_FRAGMENT_SHADER, kFrag);
    if (!vs || !fs) return false;
    mProg = g.CreateProgram();
    g.AttachShader(mProg, vs);
    g.AttachShader(mProg, fs);
    g.LinkProgram(mProg);
    g.DeleteShader(vs);
    g.DeleteShader(fs);
    GLint ok = 0;
    g.GetProgramiv(mProg, GL_LINK_STATUS, &ok);
    if (!ok) {
        diag::error("SkyRenderer: program link failed");
        g.DeleteProgram(mProg);
        mProg = 0;
        return false;
    }
    mLocPos = g.GetAttribLocation(mProg, "aPos");
    mLocTex = g.GetUniformLocation(mProg, "uTex");
    mLocOpacity = g.GetUniformLocation(mProg, "uOpacity");
    return true;
}

void SkyRenderer::ensureTexture(int w, int h) {
    if (w == mTexW && h == mTexH) return;
    g.BindTexture(GL_TEXTURE_2D, mTex);
    g.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    g.BindTexture(GL_TEXTURE_2D, 0);
    mTexW = w;
    mTexH = h;
    mUploadedSerial = 0;
}

void SkyRenderer::draw(const uint8_t* rgba, int w, int h, uint64_t serial, float opacity) {
    if (!mReady || !rgba || w <= 0 || h <= 0) return;

    // Only draw on the default framebuffer
    GLint fbo = 0;
    g.GetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    if (fbo != 0) return;

    // Save minimal state
    GLint viewport[4] = {};
    g.GetIntegerv(GL_VIEWPORT, viewport);

    ensureTexture(w, h);

    if (serial != mUploadedSerial) {
        g.BindTexture(GL_TEXTURE_2D, mTex);
        g.PixelStorei(0x0CF5 /*GL_UNPACK_ALIGNMENT*/, 1);
        g.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        g.BindTexture(GL_TEXTURE_2D, 0);
        mUploadedSerial = serial;
    }

    g.UseProgram(mProg);
    g.Uniform1i(mLocTex, 0);
    g.Uniform1f(mLocOpacity, opacity);

    if (g.ActiveTexture) g.ActiveTexture(0x84C0 /*GL_TEXTURE0*/);
    g.BindTexture(GL_TEXTURE_2D, mTex);

    g.BindBuffer(GL_ARRAY_BUFFER, mVbo);
    g.EnableVertexAttribArray((GLuint)mLocPos);
    g.VertexAttribPointer((GLuint)mLocPos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Sky: always pass depth test, write far depth so world draws on top
    g.Enable(GL_DEPTH_TEST);
    g.DepthFunc(GL_ALWAYS);
    g.DepthMask(GL_TRUE);
    g.Disable(GL_CULL_FACE);
    g.Disable(GL_SCISSOR_TEST);
    if (opacity < 0.999f) {
        g.Enable(GL_BLEND);
        g.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        g.Disable(GL_BLEND);
    }
    g.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    g.DrawArrays(GL_TRIANGLES, 0, 6);

    // Restore basics the game expects
    g.DepthFunc(0x0203 /*GL_LEQUAL*/); // common default; game may set its own next
    g.DisableVertexAttribArray((GLuint)mLocPos);
    g.BindBuffer(GL_ARRAY_BUFFER, 0);
    g.BindTexture(GL_TEXTURE_2D, 0);
    g.UseProgram(0);
    g.Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    while (g.GetError() != GL_NO_ERROR) {}
}

} // namespace customskies

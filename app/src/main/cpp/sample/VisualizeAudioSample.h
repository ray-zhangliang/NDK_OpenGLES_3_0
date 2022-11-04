#pragma once

#define MAX_AUDIO_LEVEL 2000
#define RESAMPLE_LEVEL  112

#include <detail/type_mat.hpp>
#include <detail/type_mat4x4.hpp>
#include <mutex>
#include "GLSampleBase.h"
#include <gtc/matrix_transform.hpp>
#include <thread>
#include "../util/GLUtils.h"

using namespace glm;

class VisualizeAudioSample : public GLSampleBase {
public:
    VisualizeAudioSample() {
        m_MVPMatLoc = GL_NONE;

        m_VaoId = GL_NONE;
        memset(m_VboIds, 0, sizeof(GLuint) * 2);
    }

    virtual ~VisualizeAudioSample() = default;

    virtual void Init() {
        if (m_ProgramObj)
            return;
        char vShaderStr[] =
                "#version 300 es\n"
                "layout(location = 0) in vec4 a_position;\n"
                "layout(location = 1) in vec2 a_texCoord;\n"
                "uniform mat4 u_MVPMatrix;\n"
                "out vec2 v_texCoord;\n"
                "void main()\n"
                "{\n"
                "    gl_Position = u_MVPMatrix * a_position;\n"
                "    v_texCoord = a_texCoord;\n"
                "    gl_PointSize = 4.0f;\n"
                "}";

        char fShaderStr[] =
                "#version 300 es                                     \n"
                "precision mediump float;                            \n"
                "in vec2 v_texCoord;                                 \n"
                "layout(location = 0) out vec4 outColor;             \n"
                "uniform float drawType;                             \n"
                "void main()                                         \n"
                "{                                                   \n"
                "  if(drawType == 1.0)                               \n"
                "  {                                                 \n"
                "      outColor = vec4(1, 0, 0, 1.0); \n"
                "  }                                                 \n"
                "  else                                              \n"
                "  {                                                 \n"
                "      outColor = vec4(1.0, 1.0, 1.0, 1.0);          \n"
                "  }                                                 \n"
                "}                                                   \n";

        m_ProgramObj = GLUtils::CreateProgram(vShaderStr, fShaderStr, m_VertexShader,
                                              m_FragmentShader);
        if (m_ProgramObj) {
            m_MVPMatLoc = glGetUniformLocation(m_ProgramObj, "u_MVPMatrix");
        } else {
            LOGCATE("VisualizeAudioSample::Init create program fail");
        }

        //upload RGBA image data
//    glActiveTexture(GL_TEXTURE0);
//    glBindTexture(GL_TEXTURE_2D, m_TextureId);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_RenderImage.width, m_RenderImage.height, 0, GL_RGBA,
//                 GL_UNSIGNED_BYTE, m_RenderImage.ppPlane[0]);
//    glBindTexture(GL_TEXTURE_2D, GL_NONE);

    }

    virtual void UpdateTransformMatrix(float rotateX, float rotateY, float scaleX, float scaleY) {
        m_Mutex.lock();
        GLSampleBase::UpdateTransformMatrix(rotateX, rotateY, scaleX, scaleY);

        int angleX = 0;
        int angleY = 0;

        //转化为弧度角
        float radiansX = static_cast<float>(MATH_PI / 180.0f * angleX);
        float radiansY = static_cast<float>(MATH_PI / 180.0f * angleY);

        // Projection matrix
        glm::mat4 Projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
        //glm::mat4 Projection = glm::frustum(-ratio, ratio, -1.0f, 1.0f, 4.0f, 100.0f);
        //glm::mat4 Projection = glm::perspective(45.0f, ratio, 0.1f, 100.f);

        // View matrix
        glm::mat4 View = glm::lookAt(
                glm::vec3(0, 0, 4), // Camera is at (0,0,1), in World Space
                glm::vec3(0, 0, 0), // and looks at the origin
                glm::vec3(0, 1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
        );

        // Model matrix
        glm::mat4 Model = glm::mat4(1.0f);
        Model = glm::scale(Model, glm::vec3(1.0f, 1.0f, 1.0f));
        Model = glm::rotate(Model, radiansX, glm::vec3(1.0f, 0.0f, 0.0f));
        Model = glm::rotate(Model, radiansY, glm::vec3(0.0f, 1.0f, 0.0f));

        float radiansZ = static_cast<float>(MATH_PI / 2.0f);

        //Model = glm::rotate(Model, radiansZ, glm::vec3(0.0f, 0.0f, 1.0f));
        Model = glm::translate(Model, glm::vec3(0.0f, 0.0f, 0.0f));

        m_MVPMatrix = Projection * View * Model;
        m_Mutex.unlock();
    }

    virtual void LoadShortArrData(short *const pShortArr, int arrSize) {
        static int frameIndex = 0;
        //GLSampleBase::LoadShortArrData(pShortArr, arrSize);
        LOGCATE("VisualizeAudioSample::LoadShortArrData pShortArr=%p, arrSize=%d", pShortArr,
                arrSize);
        if (pShortArr == nullptr || arrSize == 0)
            return;
        frameIndex++;
        std::unique_lock<std::mutex> lock(m_Mutex);
        if (frameIndex == 1) {
            m_audioData = new short[arrSize * 2];
            memcpy(m_audioData, pShortArr, sizeof(short) * arrSize);
            m_audioDataSize = arrSize;
            m_RenderDataSize = m_audioDataSize / RESAMPLE_LEVEL;
            m_verticesCoords = new vec3[m_RenderDataSize * 6]; //(x,y,z) * 6 points
            m_textureCoords = new vec2[m_RenderDataSize * 6]; //(x,y) * 6 points
            return;
        } else {
            if (frameIndex == 2) {
                memcpy(m_audioData + arrSize, pShortArr, sizeof(short) * arrSize);
            } else {
                memcpy(m_audioData, m_audioData + arrSize, sizeof(short) * arrSize);
                memcpy(m_audioData + arrSize, pShortArr, sizeof(short) * arrSize);
            }
            LOGCATE("VisualizeAudioSample::Draw() m_bAudioDataReady = true");
            m_bAudioDataReady = true;
            m_curAudioData = m_audioData;
            m_Cond.wait(lock);
        }
    }

    void UpdateMesh() {


        float dy = 0.5f / MAX_AUDIO_LEVEL;
        float dx = 1.0f / m_RenderDataSize;
        for (int i = 0; i < m_RenderDataSize; ++i) {
            int index = i * RESAMPLE_LEVEL;
            float y = m_curAudioData[index] * dy * -1;
            y = y < 0 ? y : -y;
            vec2 p1(i * dx, 0 + 1.0f);
            vec2 p2(i * dx, y + 1.0f);
            vec2 p3((i + 1) * dx, y + 1.0f);
            vec2 p4((i + 1) * dx, 0 + 1.0f);

            m_verticesCoords[i * 6 + 0] = GLUtils::texCoordToVertexCoord(p1);
            m_verticesCoords[i * 6 + 1] = GLUtils::texCoordToVertexCoord(p2);
            m_verticesCoords[i * 6 + 2] = GLUtils::texCoordToVertexCoord(p4);
            m_verticesCoords[i * 6 + 3] = GLUtils::texCoordToVertexCoord(p4);
            m_verticesCoords[i * 6 + 4] = GLUtils::texCoordToVertexCoord(p2);
            m_verticesCoords[i * 6 + 5] = GLUtils::texCoordToVertexCoord(p3);

            LOGCATE("%f", m_verticesCoords[i * 6 + 0].x);
            int a = 0;
        }

        m_bAudioDataReady = false;
        m_Cond.notify_all();
    }

    virtual void Draw(int screenW, int screenH) {
        std::unique_lock<std::mutex> lock(m_Mutex);

        LOGCATE("VisualizeAudioSample::Draw()");
        glClearColor(1.0f, 1.0f, 1.0f, 1.0);
        if (m_ProgramObj == GL_NONE) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        if (!m_bAudioDataReady) return;
        UpdateMesh();

        // Generate VBO Ids and load the VBOs with data
        if (m_VboIds[0] == 0) {
            glGenBuffers(2, m_VboIds);

            glGenVertexArrays(1, &m_VaoId);
            glBindVertexArray(m_VaoId);

            glBindBuffer(GL_ARRAY_BUFFER, m_VboIds[0]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * m_RenderDataSize * 6 * 3,
                         m_verticesCoords, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const void *) 0);
            glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

            glBindBuffer(GL_ARRAY_BUFFER, m_VboIds[1]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * m_RenderDataSize * 6 * 2,
                         m_textureCoords, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const void *) 0);
            glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

            glBindVertexArray(GL_NONE);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, m_VboIds[0]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * m_RenderDataSize * 6 * 3,
                            m_verticesCoords);

            glBindBuffer(GL_ARRAY_BUFFER, m_VboIds[1]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * m_RenderDataSize * 6 * 2,
                            m_textureCoords);
        }

        // Use the program object
        glUseProgram(m_ProgramObj);
        glBindVertexArray(m_VaoId);
        glUniformMatrix4fv(m_MVPMatLoc, 1, GL_FALSE, &m_MVPMatrix[0][0]);
        GLUtils::setFloat(m_ProgramObj, "drawType", 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, m_RenderDataSize * 6);
        GLUtils::setFloat(m_ProgramObj, "drawType", 0.0f);
        glDrawArrays(GL_LINES, 0, m_RenderDataSize * 6);
    }

    virtual void Destroy() {
        if (m_ProgramObj) {
            glDeleteProgram(m_ProgramObj);
            glDeleteBuffers(2, m_VboIds);
            glDeleteVertexArrays(1, &m_VaoId);
        }
    }

private:
    GLint m_MVPMatLoc;
    glm::mat4 m_MVPMatrix;

    GLuint m_VaoId;
    GLuint m_VboIds[2];

    short *m_audioData = nullptr;
    int m_audioDataSize = 0;

    short *m_curAudioData = nullptr;

    std::mutex m_Mutex;
    std::condition_variable m_Cond;

    vec3 *m_verticesCoords = nullptr;
    vec2 *m_textureCoords = nullptr;
    int m_RenderDataSize;

    volatile bool m_bAudioDataReady = false;
};

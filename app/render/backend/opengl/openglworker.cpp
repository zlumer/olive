#include "openglworker.h"

#include "functions.h"
#include "node/node.h"
#include "render/pixelservice.h"

OpenGLWorker::OpenGLWorker(QOpenGLContext *share_ctx, OpenGLShaderCache *shader_cache, DecoderCache *decoder_cache, VideoRenderFrameCache *frame_cache, QObject *parent) :
  VideoRenderWorker(decoder_cache, frame_cache, parent),
  share_ctx_(share_ctx),
  ctx_(nullptr),
  functions_(nullptr),
  shader_cache_(shader_cache)
{
  surface_.create();
}

OpenGLWorker::~OpenGLWorker()
{
  surface_.destroy();
}

bool OpenGLWorker::InitInternal()
{
  if (!VideoRenderWorker::InitInternal()) {
    return false;
  }

  // Create context object
  ctx_ = new QOpenGLContext();

  // Set share context
  ctx_->setShareContext(share_ctx_);

  // Create OpenGL context (automatically destroys any existing if there is one)
  if (!ctx_->create()) {
    qWarning() << "Failed to create OpenGL context in thread" << thread();
    return false;
  }

  ctx_->moveToThread(this->thread());

  //qDebug() << "Processor initialized in thread" << thread() << "- context is in" << ctx_->thread();

  // The rest of the initialization needs to occur in the other thread, so we signal for it to start
  QMetaObject::invokeMethod(this, "FinishInit", Qt::QueuedConnection);

  return true;
}

QVariant OpenGLWorker::FrameToValue(FramePtr frame)
{
  OpenGLTexturePtr footage_tex = std::make_shared<OpenGLTexture>();
  footage_tex->Create(ctx_, frame);

  // OCIO's CPU conversion is more accurate, so for online we render on CPU but offline we render GPU
  //if (video_params().mode() == olive::kOnline) {
    // Convert frame to float for
    //frame = PixelService::ConvertPixelFormat(frame, olive::PIX_FMT_RGBA32F);
  //}

  // FIXME: Alpha association and color management

  return QVariant::fromValue(footage_tex);
}

bool OpenGLWorker::OutputIsAccelerated(NodeOutput* output)
{
  return shader_cache_->HasShader(output);
}

void OpenGLWorker::CloseInternal()
{
  buffer_.Destroy();

  functions_ = nullptr;
  delete ctx_;
}

void OpenGLWorker::ParametersChangedEvent()
{
  if (functions_ != nullptr && video_params().is_valid()) {
    functions_->glViewport(0, 0, video_params().effective_width(), video_params().effective_height());
  }
}

QVariant OpenGLWorker::RunNodeAccelerated(NodeOutput *out)
{
  OpenGLShaderPtr shader = shader_cache_->GetShader(out);
  Node* node = out->parent();

  // Create the output texture
  OpenGLTexturePtr output = std::make_shared<OpenGLTexture>();
  output->Create(ctx_, video_params().effective_width(), video_params().effective_height(), video_params().format());

  buffer_.Attach(output);

  buffer_.Bind();

  shader->bind();

  unsigned int input_texture_count = 0;

  foreach (NodeParam* param, node->parameters()) {
    if (param->type() == NodeParam::kInput) {
      // See if the shader has takes this parameter as an input
      int variable_location = shader->uniformLocation(param->id());

      if (variable_location > -1) {
        // This variable is used in the shader, let's set it to our value

        NodeInput* input = static_cast<NodeInput*>(param);
        switch (input->data_type()) {
        case NodeInput::kInt:
          shader->setUniformValue(variable_location, input->value().toInt());
          break;
        case NodeInput::kFloat:
          shader->setUniformValue(variable_location, input->value().toFloat());
          break;
        case NodeInput::kVec2:
          shader->setUniformValue(variable_location, input->value().value<QVector2D>());
          break;
        case NodeInput::kVec3:
          shader->setUniformValue(variable_location, input->value().value<QVector3D>());
          break;
        case NodeInput::kVec4:
          shader->setUniformValue(variable_location, input->value().value<QVector4D>());
          break;
        case NodeInput::kMatrix:
          shader->setUniformValue(variable_location, input->value().value<QMatrix4x4>());
          break;
        case NodeInput::kColor:
          shader->setUniformValue(variable_location, input->value().value<QColor>());
          break;
        case NodeInput::kBoolean:
          shader->setUniformValue(variable_location, input->value().toBool());
          break;
        case NodeInput::kTexture:
        case NodeInput::kFootage:
        {
          OpenGLTexturePtr texture = input->value().value<OpenGLTexturePtr>();

          functions_->glActiveTexture(GL_TEXTURE0 + input_texture_count);

          if (texture == nullptr) {
            functions_->glBindTexture(GL_TEXTURE_2D, 0);
          } else {
            functions_->glBindTexture(GL_TEXTURE_2D, texture->texture());
          }

          // Set value to bound texture
          shader->setUniformValue(variable_location, input_texture_count);

          input_texture_count++;
          break;
        }
        case NodeInput::kAny:
        case NodeInput::kSamples:
        case NodeInput::kTrack:
        case NodeInput::kString:
        case NodeInput::kRational:
        case NodeInput::kBlock:
        case NodeInput::kFont:
        case NodeInput::kFile:
        case NodeInput::kNone:
          break;
        }
      }
    }
  }

  //qDebug() << "    Blitting with shader!";
  olive::gl::Blit(shader);

  // Release any textures we bound before
  while (input_texture_count > 0) {
    input_texture_count--;

    // Release texture here
    functions_->glActiveTexture(GL_TEXTURE0 + input_texture_count);
    functions_->glBindTexture(GL_TEXTURE_2D, 0);
  }

  shader->release();

  buffer_.Release();

  buffer_.Detach();

  functions_->glFinish();

  return QVariant::fromValue(output);
}

void OpenGLWorker::TextureToBuffer(const QVariant &tex_in, QByteArray &buffer)
{
  OpenGLTexturePtr texture = tex_in.value<OpenGLTexturePtr>();

  PixelFormatInfo format_info = PixelService::GetPixelFormatInfo(video_params().format());

  QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
  buffer_.Attach(texture);
  f->glBindFramebuffer(GL_READ_FRAMEBUFFER, buffer_.buffer());

  f->glReadPixels(0,
                  0,
                  texture->width(),
                  texture->height(),
                  format_info.pixel_format,
                  format_info.gl_pixel_type,
                  buffer.data());

  f->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  buffer_.Detach();
}

void OpenGLWorker::FinishInit()
{
  // Make context current on that surface
  if (!ctx_->makeCurrent(&surface_)) {
    qWarning() << "Failed to makeCurrent() on offscreen surface in thread" << thread();
    return;
  }

  // Store OpenGL functions instance
  functions_ = ctx_->functions();

  // Set up OpenGL parameters as necessary
  functions_->glEnable(GL_BLEND);
  ParametersChangedEvent();

  buffer_.Create(ctx_);
}

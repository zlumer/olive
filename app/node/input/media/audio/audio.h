#ifndef AUDIOINPUT_H
#define AUDIOINPUT_H

#include "../media.h"

class AudioInput : public MediaInput
{
public:
  AudioInput();

  virtual Node* copy() override;

  virtual QString Name() override;
  virtual QString id() override;
  virtual QString Category() override;
  virtual QString Description() override;

  NodeOutput* samples_output();

protected:
  virtual QVariant Value(NodeOutput* output) override;

private:
  NodeOutput* samples_output_;
};

#endif // AUDIOINPUT_H

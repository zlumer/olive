/***

  Olive - Non-Linear Video Editor
  Copyright (C) 2019 Olive Team

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

***/

#ifndef VIEWER_H
#define VIEWER_H

#include "node/node.h"
#include "render/videoparams.h"
#include "render/audioparams.h"

/**
 * @brief A bridge between a node system and a ViewerPanel
 *
 * Receives update/time change signals from ViewerPanels and responds by sending them a texture of that frame
 */
class ViewerOutput : public Node
{
  Q_OBJECT
public:
  ViewerOutput();

  virtual Node* copy() override;

  virtual QString Name() override;
  virtual QString id() override;
  virtual QString Category() override;
  virtual QString Description() override;

  NodeInput* texture_input();
  NodeInput* samples_input();
  NodeInput* length_input();

  virtual void InvalidateCache(const rational &start_range, const rational &end_range, NodeInput *from = nullptr) override;

  const VideoParams& video_params();
  const AudioParams& audio_params();

  void set_video_params(const VideoParams& video);
  void set_audio_params(const AudioParams& audio);

  rational Length();

protected:
  virtual void DependentEdgeChanged(NodeInput* from) override;

signals:
  void TimebaseChanged(const rational&);

  void VideoChangedBetween(const rational&, const rational&);

  void AudioChangedBetween(const rational&, const rational&);

  void VideoGraphChanged();

  void AudioGraphChanged();

  void LengthChanged(const rational& length);

  void SizeChanged(int width, int height);

private:
  NodeInput* texture_input_;

  NodeInput* samples_input_;

  NodeInput* length_input_;

  rational timebase_;

  VideoParams video_params_;

  AudioParams audio_params_;

};

#endif // VIEWER_H

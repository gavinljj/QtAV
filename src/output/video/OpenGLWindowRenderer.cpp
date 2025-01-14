/******************************************************************************
    QtAV:  Media play library based on Qt and FFmpeg
    Copyright (C) 2014-2015 Wang Bin <wbsecg1@gmail.com>

*   This file is part of QtAV

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#include "QtAV/OpenGLWindowRenderer.h"
#include "QtAV/private/OpenGLRendererBase_p.h"
#include "QtAV/private/factory.h"
#include "QtAV/private/mkid.h"
#include <QResizeEvent>
#include "utils/Logger.h"

namespace QtAV {
VideoRendererId VideoRendererId_OpenGLWindow = mkid::id32base36_6<'Q', 'O', 'G', 'L', 'W', 'w'>::value;
FACTORY_REGISTER(VideoRenderer, OpenGLWindow, "OpenGLWindow")

class OpenGLWindowRendererPrivate : public OpenGLRendererBasePrivate
{
public:
    OpenGLWindowRendererPrivate(QPaintDevice* pd)
        : OpenGLRendererBasePrivate(pd)
    {}
};

OpenGLWindowRenderer::OpenGLWindowRenderer(UpdateBehavior updateBehavior, QWindow *parent):
    QOpenGLWindow(updateBehavior, parent)
  , OpenGLRendererBase(*new OpenGLWindowRendererPrivate(this))
{
}

VideoRendererId OpenGLWindowRenderer::id() const
{
    return VideoRendererId_OpenGLWindow;
}

void OpenGLWindowRenderer::onUpdate()
{
    // MUST call update() on gui(main) thread that the window belongs to because update() will finally call startTimer
    updateUi();
}

void OpenGLWindowRenderer::initializeGL()
{
    onInitializeGL();
}

void OpenGLWindowRenderer::paintGL()
{
    onPaintGL();
}

void OpenGLWindowRenderer::resizeGL(int w, int h)
{
    onResizeGL(w, h);
}

void OpenGLWindowRenderer::resizeEvent(QResizeEvent *e)
{
    onResizeEvent(e->size().width(), e->size().height());
    QOpenGLWindow::resizeEvent(e); //will call resizeGL(). TODO:will call paintEvent()?
}

} //namespace QtAV

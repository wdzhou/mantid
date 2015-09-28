#include "MantidQtMantidWidgets/InputController.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QCursor>
#include <QApplication>

#include <iostream>

namespace MantidQt {
namespace MantidWidgets {

//--------------------------------------------------------------------------------

InputController::InputController(QObject *parent, bool contextAllowed) :
QObject(parent),m_canShowContextMenu(contextAllowed)
{
}

//--------------------------------------------------------------------------------

/**
  * Constructor.
  * @param parent :: The parent object.
  */
InputController3DMove::InputController3DMove(QObject *parent):
InputController(parent,false),
m_isButtonPressed(false)
{
}

/**
  * Process the mouse press event.
  * Send out movement initialization signals.
  */
void InputController3DMove::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::MidButton)
    {
      emit initZoom(event->x(),event->y());
      m_isButtonPressed=true;
    }
    else if (event->buttons() & Qt::LeftButton)
    {
      emit initRotation(event->x(),event->y());
      m_isButtonPressed=true;
    }
    else if(event->buttons() & Qt::RightButton)
    {
      emit initTranslation(event->x(),event->y());
      m_isButtonPressed=true;
    }
}

/**
  * Process the mouse move event.
  * Send out surface movement signals.
  */
void InputController3DMove::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
      emit rotate(event->x(),event->y());
    }
    else if(event->buttons() & Qt::RightButton)
    {
      emit translate(event->x(),event->y());
    }
    else if(event->buttons() & Qt::MidButton)
    {
      emit zoom(event->x(),event->y());
    }
}

/**
  * Process the mouse release event.
  * Finalize the interaction.
  */
void InputController3DMove::mouseReleaseEvent(QMouseEvent *)
{
    m_isButtonPressed = false;
    emit finish();
}

/**
  * Process the mouse wheel event.
  * Send the wheel zoom signal.
  */
void InputController3DMove::wheelEvent(QWheelEvent *event)
{
    emit wheelZoom( event->x(), event->y(), event->delta() );
}

//--------------------------------------------------------------------------------

/**
  * Constructor.
  * @param parent :: The parent object.
  */
InputControllerPick::InputControllerPick(QObject *parent):
InputController(parent),
m_isButtonPressed(false)
{
}

/**
  * Process the mouse press event.
  */
void InputControllerPick::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
      m_isButtonPressed = true;
      m_rect.setRect( event->x(), event->y(), 1, 1 );
      emit pickPointAt( event->x(), event->y() );
    }
}

/**
  * Process the mouse move event.
  */
void InputControllerPick::mouseMoveEvent(QMouseEvent *event)
{
    if ( m_isButtonPressed )
    {
      m_rect.setBottomRight(QPoint( event->x(), event->y() ));
      emit setSelection(m_rect);
    }
    else
    {
      emit touchPointAt( event->x(), event->y() );
    }
}

/**
  * Process the mouse release event.
  */
void InputControllerPick::mouseReleaseEvent(QMouseEvent *)
{
    m_isButtonPressed = false;
    emit finishSelection();
}

//--------------------------------------------------------------------------------

/**
  * Constructor.
  */
InputControllerDrawShape::InputControllerDrawShape(QObject *parent):
InputController(parent),
m_creating(false),
m_x(0),
m_y(0),
m_shapeType(),
m_isButtonPressed(false)
{
}

/**
  * Process the mouse press event. Sends addShape or selectAt signal.
  */
void InputControllerDrawShape::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
      m_isButtonPressed = true;
      if ( m_creating && !m_shapeType.isEmpty() )
      {
        emit addShape( m_shapeType, event->x(), event->y(), m_borderColor, m_fillColor );
      }
      else if ( event->modifiers() & Qt::ControlModifier )
      {
          emit selectCtrlAt( event->x(), event->y() );
      }
      else
      {
          emit selectAt( event->x(), event->y() );
      }
      m_x = event->x();
      m_y = event->y();
      m_rect.setRect( event->x(), event->y(), 1, 1 );
    }
}

/**
  * Process the mouse move event. If the left mouse button is down sends editing signals.
  */
void InputControllerDrawShape::mouseMoveEvent(QMouseEvent *event)
{
    if ( m_isButtonPressed )
    {
        if ( m_creating )
        {
            emit moveRightBottomTo( event->x(), event->y() );
        }
        else
        {
            emit moveBy( event->x() - m_x, event->y() - m_y );
            m_rect.setBottomRight(QPoint( event->x(), event->y() ));
            m_x = event->x();
            m_y = event->y();
            emit setSelection( m_rect );
        }
    }
    else
    {
        emit touchPointAt( event->x(), event->y() );
    }
}

/**
  * Process the mouse button release event.
  */
void InputControllerDrawShape::mouseReleaseEvent(QMouseEvent *)
{
    m_isButtonPressed = false;
    m_creating = false;
    m_shapeType = "";
    emit finishSelection( m_rect );
}

/**
  * Process the keyboard key press event.
  */
void InputControllerDrawShape::keyPressEvent(QKeyEvent *event)
{
    switch(event->key())
    {
    case Qt::Key_Delete:
    case Qt::Key_Backspace: emit removeSelectedShapes(); break;
    }
}

/**
  * Process event of the mouse leaving the widget.
  */
void InputControllerDrawShape::leaveEvent(QEvent *)
{
    emit restoreOverrideCursor();
}

/**
  * Slot for defining the shape to draw and initializing drawing.
  */
void InputControllerDrawShape::startCreatingShape2D(const QString& type,const QColor& borderColor,const QColor& fillColor)
{
  m_creating = true;
  m_shapeType = type;
  m_borderColor = borderColor;
  m_fillColor = fillColor;
}

/**
 * Action on disabling.
 */
void InputControllerDrawShape::onDisabled()
{
    m_creating = false;
    emit disabled();
}
//--------------------------------------------------------------------------------

/**
  * Constructor.
  */
InputControllerMoveUnwrapped::InputControllerMoveUnwrapped(QObject *parent):
InputController(parent,false),
m_isButtonPressed(false)
{
}

/**
  * Process the mouse press event.
  */
void InputControllerMoveUnwrapped::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_isButtonPressed = true;
        m_rect.setTopLeft(QPoint(event->x(),event->y()));
    }
    else if (event->button() == Qt::RightButton)
    {
        emit unzoom();
    }
}

/**
  * Process the mouse move event.
  */
void InputControllerMoveUnwrapped::mouseMoveEvent(QMouseEvent *event)
{
    if ( m_isButtonPressed )
    {
        m_rect.setBottomRight(QPoint(event->x(),event->y()));
        emit setSelectionRect(m_rect);
    }
}

/**
  * Process the mouse button release event.
  */
void InputControllerMoveUnwrapped::mouseReleaseEvent(QMouseEvent *)
{
    if ( m_isButtonPressed )
    {
        emit zoom();
    }
    m_isButtonPressed = false;
}

//--------------------------------------------------------------------------------

/**
  * Constructor.
  */
InputControllerDraw::InputControllerDraw(QObject *parent):
InputController(parent),
m_max_size(32),
m_size(30),
m_isButtonPressed(false),
m_isActive(false),
m_cursor(NULL)
{
}

InputControllerDraw::~InputControllerDraw()
{
    delete m_cursor;
}

/**
  * Process the mouse press event.
  */
void InputControllerDraw::mousePressEvent(QMouseEvent *event)
{
    m_isActive = true;
    setPosition(QPoint(event->x(),event->y()));
    if (event->button() == Qt::LeftButton)
    {
        m_isButtonPressed = true;
        signalLeftClick();
    }
}

/**
  * Process the mouse move event.
  */
void InputControllerDraw::mouseMoveEvent(QMouseEvent *event)
{
    m_isActive = true;
    setPosition(QPoint(event->x(),event->y()));
    if ( m_isButtonPressed )
    {
        signalLeftClick();
    }
}

/**
  * Process the mouse button release event.
  */
void InputControllerDraw::mouseReleaseEvent(QMouseEvent *)
{
    m_isButtonPressed = false;
}

void InputControllerDraw::wheelEvent(QWheelEvent *event)
{
    int d = m_size + ( event->delta() > 0 ? 4 : -4 );
    if ( d > 2 && d < m_max_size )
    {
        m_size = d;
        redrawCursor();
        QApplication::restoreOverrideCursor();
        QApplication::setOverrideCursor(QCursor( *m_cursor, 0, 0 ));
    }
}

void InputControllerDraw::enterEvent(QEvent *)
{
    redrawCursor();
    QApplication::setOverrideCursor(QCursor( *m_cursor, 0, 0 ));
    m_isActive = true;
}

void InputControllerDraw::leaveEvent(QEvent *)
{
    QApplication::restoreOverrideCursor();
    m_isActive = false;
}

void InputControllerDraw::redrawCursor()
{
  if (!m_cursor)
  {
    m_cursor = new QPixmap(m_max_size,m_max_size);
  }
  drawCursor(m_cursor);
}

//--------------------------------------------------------------------------------

InputControllerErase::InputControllerErase(QObject *parent): InputControllerDraw(parent),
  m_rect( 0, 0, cursorSize(), cursorSize() ) 
{
  m_image = new QPixmap(":/PickTools/eraser.png");
}

InputControllerErase::~InputControllerErase()
{
  delete m_image;
}

void InputControllerErase::signalLeftClick()
{
  emit erase(m_rect);
}

void InputControllerErase::onPaint(QPainter& painter)
{
    if ( isActive() && !isButtonPressed() )
    {
        painter.drawPixmap(m_rect.bottomRight(),*m_image);
    }
}

void InputControllerErase::drawCursor(QPixmap *cursor)
{
    cursor->fill(QColor(255,255,255,0));
    QPainter painter( cursor );
    auto size = cursorSize();

    auto pen = QPen(Qt::DashLine);
    QVector<qreal> dashPattern;
    dashPattern << 4 << 4;
    pen.setDashPattern(dashPattern);
    pen.setColor(QColor(0,0,0));
    painter.setPen(pen);
    painter.drawRect( QRect( 0, 0, size, size ) );

    pen.setColor(QColor(255,255,255));
    pen.setDashOffset(4);
    painter.setPen(pen);
    painter.drawRect( QRect( 0, 0, size, size ) );
}

void InputControllerErase::setPosition(const QPoint &pos)
{
  m_rect.moveTopLeft(pos);
}

void InputControllerErase::resize()
{
    auto size = cursorSize();
    m_rect.setSize( QSize(size, size) );
}

}
}


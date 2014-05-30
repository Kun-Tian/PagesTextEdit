#include "PagesTextEdit.h"

#include <QPainter>
#include <QPaintEvent>
#include <QDebug>
#include <QScrollBar>
#include <QTextFrame>
#include <QTextFrameFormat>


PagesTextEdit::PagesTextEdit(QWidget *parent) :
	QTextEdit(parent),
	m_usePageMode(false),
	m_zoomRange(0)
{
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

bool PagesTextEdit::usePageMode() const
{
	return m_usePageMode;
}

void PagesTextEdit::setZoomRange(int _zoomRange)
{
    // Q_ASSERT_X will be raised
    if (_zoomRange <= -10) return;

	//
	// Отменяем предыдущее мастштабирование
	//
	qreal zoomRange = (qreal)(m_zoomRange + 10) / 10;
	privateZoomOut(zoomRange);

	//
	// Обновляем коэффициент
	//
	m_zoomRange = _zoomRange;
	zoomRange = (qreal)(m_zoomRange + 10) / 10;

	//
	// Масштабируем с новым коэффициентом
	//
	{
		//
		// ... шрифт
		//
		privateZoomIn(zoomRange);

		//
		// ... документ
		//
		m_pageMetrics.zoomIn(zoomRange);
	}

	//
	// Уведомляем о том, что коэффициент изменился
	//
	emit zoomRangeChanged(m_zoomRange);
}

void PagesTextEdit::setUsePageMode(bool _use)
{
	if (m_usePageMode != _use) {
		m_usePageMode = _use;

		//
		// Перерисуем себя
		//
		repaint();
	}
}

void PagesTextEdit::setPageFormat(QPageSize::PageSizeId _pageFormat)
{
	m_pageMetrics.update(_pageFormat);

	//
	// Перерисуем себя
	//
	repaint();
}

void PagesTextEdit::setPageMargins(const QMarginsF& _margins)
{
	m_pageMetrics.update(m_pageMetrics.pageFormat(), _margins);

	//
	// Перерисуем себя
	//
	repaint();
}

void PagesTextEdit::paintEvent(QPaintEvent* _event)
{
	updateInnerGeometry();

	updateVerticalScrollRange();

	paintPagesView();

	QTextEdit::paintEvent(_event);
}

void PagesTextEdit::resetZoom()
{
	setZoomRange(0);
}

void PagesTextEdit::wheelEvent(QWheelEvent* _event)
{
	if (_event->modifiers() & Qt::ControlModifier) {
		if (_event->orientation() == Qt::Vertical) {
			//
			// zoomRange > 0 - Текст увеличивается
			// zoomRange < 0 - Текст уменьшается
			//
			int zoomRange = m_zoomRange + (_event->angleDelta().y() / 120);
			setZoomRange(zoomRange);

			_event->accept();
		}
	} else {
		QTextEdit::wheelEvent(_event);
	}
}

void PagesTextEdit::updateInnerGeometry()
{
	//
	// Формируем параметры отображения
	//
	QSizeF documentSize(width() - verticalScrollBar()->width(), -1);
	QMargins viewportMargins;
	QMarginsF rootFrameMargins = m_pageMetrics.pxPageMargins();

	if (m_usePageMode) {
		//
		// Настроить размер документа
		//

		int pageWidth = m_pageMetrics.pxPageSize().width();
		int pageHeight = m_pageMetrics.pxPageSize().height();
		documentSize = QSizeF(pageWidth, pageHeight);

		//
		// Рассчитываем отступы для viewport
		//
		if (width() > pageWidth) {
			viewportMargins =
					QMargins(
						(width() - pageWidth - verticalScrollBar()->width() - 2)/2,
						20,
						(width() - pageWidth - verticalScrollBar()->width() - 2)/2,
						20);
		} else {
			viewportMargins = QMargins(0, 20, 0, 20);
		}
	}

	//
	// Применяем параметры отображения
	//

	if (document()->documentMargin() != 0) {
		document()->setDocumentMargin(0);
	}

	if (document()->pageSize() != documentSize) {
		document()->setPageSize(documentSize);
	}

	setViewportMargins(viewportMargins);

	QTextFrameFormat rootFrameFormat = document()->rootFrame()->frameFormat();
	if (rootFrameFormat.leftMargin() != rootFrameMargins.left()
		|| rootFrameFormat.topMargin() != rootFrameMargins.top()
		|| rootFrameFormat.rightMargin() != rootFrameMargins.right()
		|| rootFrameFormat.bottomMargin() != rootFrameMargins.bottom()) {
		rootFrameFormat.setLeftMargin(rootFrameMargins.left());
		rootFrameFormat.setTopMargin(rootFrameMargins.top());
		rootFrameFormat.setRightMargin(rootFrameMargins.right());
		rootFrameFormat.setBottomMargin(rootFrameMargins.bottom());
		document()->rootFrame()->setFrameFormat(rootFrameFormat);
	}
}

void PagesTextEdit::updateVerticalScrollRange()
{
	//
	// В постраничном режиме показываем страницу целиком
	//
	if (m_usePageMode) {
		int maximumValue = m_pageMetrics.pxPageSize().height() * document()->pageCount() - viewport()->size().height();
		if (verticalScrollBar()->maximum() != maximumValue) {
			verticalScrollBar()->setRange(0, maximumValue);
		}
	}
	//
	// В обычном режиме просто добавляем немного дополнительной прокрутки для удобства
	//
	else {
		//
		// TODO
		//
	}
}

void PagesTextEdit::paintPagesView()
{
	//
	// Оформление рисуется только тогда, когда редактор находится в постраничном режиме
	//
	if (m_usePageMode) {
		//
		// Нарисовать линии разрыва страниц
		//

		qreal pageWidth = m_pageMetrics.pxPageSize().width();
		qreal pageHeight = m_pageMetrics.pxPageSize().height();

		QPainter p(viewport());
		QPen spacePen(palette().window(), 9);
		QPen borderPen(palette().dark(), 1);

		qreal curHeight = pageHeight - (verticalScrollBar()->value() % (int)pageHeight);
        bool can_draw_next_page_line = verticalScrollBar()->value() != verticalScrollBar()->maximum(); // next page top border fix
        int x = pageWidth - (width() % 2 == 0 ? 1 : 0); // right border resize fix

		//
		// Нарисовать верхнюю границу
		//
		if (curHeight - pageHeight >= 0) {
			p.setPen(borderPen);
			// ... верхняя
			p.drawLine(0, curHeight - pageHeight, pageWidth, curHeight - pageHeight);
		}

		while (curHeight < height()) {
			//
			// Фон разрыва страниц
			//
			p.setPen(spacePen);
			p.drawLine(0, curHeight-4, width(), curHeight-4);

			//
			// Границы страницы
			//
			p.setPen(borderPen);
			// ... нижняя
			p.drawLine(0, curHeight-8, pageWidth, curHeight-8);
			// ... верхняя следующей страницы
            if (can_draw_next_page_line)
                p.drawLine(0, curHeight, pageWidth, curHeight);
			// ... левая
			p.drawLine(0, curHeight-pageHeight, 0, curHeight-8);
			// ... правая
            p.drawLine(x, curHeight-pageHeight, x, curHeight-8);

			curHeight += pageHeight;
		}

		//
		// Нарисовать боковые границы страницы, когда страница не влезает в экран
		//
		if (curHeight >= height()) {
			//
			// Границы страницы
			//
			p.setPen(borderPen);
			// ... левая
			p.drawLine(0, curHeight-pageHeight, 0, height());
			// ... правая
            p.drawLine(x, curHeight-pageHeight, x, height());
		}
	}
}

namespace {
	/**
	 * @brief Перевести пиксели в поинты
	 */
	static qreal pixelsToPoints(const QPaintDevice* _device, qreal _pixels)
	{
		return _pixels * (qreal)72 / (qreal)_device->logicalDpiY();
	}

	/**
	 * @brief Вычислить масштабированное значение
	 */
	static qreal scale(qreal _source, qreal _scaleRange)
	{
		qreal result = _source;
		if (_scaleRange > 0) {
			result = _source * _scaleRange;
		} else if (_scaleRange < 0) {
			result = _source / _scaleRange * -1;
		} else {
			Q_ASSERT_X(0, "scale", "scale to zero range");
		}
		return result;
	}
}

void PagesTextEdit::privateZoomIn(qreal _range)
{
	//
	// Нужно увеличить на _range пикселей
	// * шрифт каждого блока
	// * формат каждого блока
	//
	QTextCursor cursor(document());
	cursor.beginEditBlock();

	do {
		//
		// Выделим редактируемый блок текста
		//
		cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

		//
		// Обновляем настройки шрифта
		//
		QTextCharFormat blockCharFormat = cursor.charFormat();
		QFont blockFont = blockCharFormat.font();
		blockFont.setPointSizeF(
					scale(blockFont.pointSizeF(), pixelsToPoints(this, _range))
					);
		blockCharFormat.setFont(blockFont);
		cursor.mergeCharFormat(blockCharFormat);
		cursor.mergeBlockCharFormat(blockCharFormat);

		//
		// Обновляем настройки позиционирования
		//
		QTextBlockFormat blockFormat = cursor.blockFormat();
		if (blockFormat.leftMargin() > 0) {
			blockFormat.setLeftMargin(scale(blockFormat.leftMargin(), _range));
		}
		if (blockFormat.topMargin() > 0) {
			blockFormat.setTopMargin(scale(blockFormat.topMargin(), _range));
		}
		if (blockFormat.rightMargin() > 0) {
			blockFormat.setRightMargin(scale(blockFormat.rightMargin(), _range));
		}
		if (blockFormat.bottomMargin() > 0) {
			blockFormat.setBottomMargin(scale(blockFormat.bottomMargin(), _range));
		}
		cursor.mergeBlockFormat(blockFormat);

		//
		// Переходим к следующему блоку
		//
		cursor.movePosition(QTextCursor::NextBlock);
	} while (!cursor.atEnd());

	cursor.endEditBlock();
}

void PagesTextEdit::privateZoomOut(qreal _range)
{
	privateZoomIn(-_range);
}



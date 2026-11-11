#include <QtTest>
#include <QApplication>
#include <QFontDatabase>
#include <QFontInfo>
#include <QScrollBar>
#include "qhexview.h"
#include "document/buffer/qmemorybuffer.h"

class RenderingTest : public QObject
{
    Q_OBJECT
private:
    static QPair<int, int> areaSpan(const QHexRenderer& renderer, int area) {
        int first = -1, last = -1;
        for (int x = 0; x < renderer.documentWidth(); ++x) {
            if (renderer.hitTestArea(QPoint(x, renderer.lineHeight() + 1)) == area) {
                if (first < 0) first = x;
                last = x;
            }
        }
        return qMakePair(first, last);
    }
    static bool isInk(QRgb pixel) { return qRed(pixel) < 120 && qGreen(pixel) < 120 && qBlue(pixel) < 120; }

private slots:
    void readableFixedFont() {
        QHexView view;
        QVERIFY2(QFontInfo(view.font()).fixedPitch(), qPrintable(QFontInfo(view.font()).family()));
        QVERIFY(view.font().pointSizeF() >= 12);
    }

    void largerSystemTextSizeIsRespected() {
        const auto previous = qApp->font();
        auto larger = previous; larger.setPointSizeF(22); qApp->setFont(larger);
        QHexView view;
        qApp->setFont(previous);
        QVERIFY(view.font().pointSizeF() >= 22);
        QVERIFY(QFontInfo(view.font()).fixedPitch());
    }

    void fontChangeRecalculatesScrollbars() {
        QHexView view;
        view.resize(500, 240);
        view.setDocument(QHexDocument::fromMemory<QMemoryBuffer>(QByteArray(4096, 'W'), &view));
        view.show(); QTest::qWait(20);
        const int previous = view.horizontalScrollBar()->maximum();
        QFont large = view.font(); large.setPointSizeF(30);
        view.setFont(large); QCoreApplication::processEvents();
        QVERIFY(view.horizontalScrollBar()->maximum() > previous);
        view.resize(6000, 1000); QCoreApplication::processEvents();
        QCOMPARE(view.horizontalScrollBar()->maximum(), 0);
        QCOMPARE(view.horizontalScrollBar()->value(), 0);
    }

    void lineWidthChangeRecalculatesScrollbars() {
        QHexView view;
        view.resize(300, 200);
        view.setDocument(QHexDocument::fromMemory<QMemoryBuffer>(QByteArray(256, 'W'), &view));
        view.show(); QTest::qWait(20);
        const int previous = view.horizontalScrollBar()->maximum();
        view.document()->setHexLineWidth(32);
        QCoreApplication::processEvents();
        QVERIFY(view.horizontalScrollBar()->maximum() > previous);
    }

    void renderingStaysInsideColumns_data() {
        QTest::addColumn<int>("points"); QTest::addColumn<qreal>("scale"); QTest::addColumn<int>("length");
        for (int points : {12, 18, 24})
            for (qreal scale : {1.0, 1.5, 2.0})
                for (int length : {0, 1, 15, 16, 17, 32})
                    QTest::newRow(qPrintable(QString("%1pt-%2x-%3bytes").arg(points).arg(scale).arg(length))) << points << scale << length;
    }

    void renderingStaysInsideColumns() {
        QFETCH(int, points); QFETCH(qreal, scale); QFETCH(int, length);
        const QFont applicationFont = qApp->font();
        QFont veryLarge = QFontDatabase::systemFont(QFontDatabase::GeneralFont); veryLarge.setPointSize(36);
        qApp->setFont(veryLarge);
        QHexView fontSource;
        QFont fixed = fontSource.font(); fixed.setPointSize(points);
        QImage device(1, 1, QImage::Format_ARGB32_Premultiplied);
        device.setDevicePixelRatio(scale);
        QScopedPointer<QHexDocument> document(QHexDocument::fromMemory<QMemoryBuffer>(QByteArray(length, char(0x88))));
        QHexRenderer renderer(document.data(), QFontMetricsF(fixed, &device));
        const auto hex = areaSpan(renderer, QHexRenderer::HexArea);
        const auto ascii = areaSpan(renderer, QHexRenderer::AsciiArea);
        QVERIFY(hex.first >= 0); QVERIFY(ascii.first > hex.second);
        const QPalette previousPalette = qApp->palette();
        QPalette palette; palette.setColor(QPalette::Base, Qt::white); palette.setColor(QPalette::Window, Qt::white);
        palette.setColor(QPalette::Text, Qt::black); palette.setColor(QPalette::WindowText, Qt::black);
        qApp->setPalette(palette);
        QImage image(qCeil((renderer.documentWidth() + 10) * scale), qCeil(renderer.lineHeight() * 5 * scale), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(scale); image.fill(Qt::white);
        QPainter painter(&image); painter.setFont(fixed); painter.setPen(Qt::black);
        renderer.render(&painter, 0, renderer.documentLines(), 0); painter.end();
        qApp->setFont(applicationFont); qApp->setPalette(previousPalette);
        // Padding between the last hex cell and ASCII must remain free of text/selection.
        for (int y = qCeil(renderer.lineHeight() * scale); y < renderer.lineHeight() * 2 * scale; ++y)
            for (int x = qCeil((hex.second + 2) * scale); x < (ascii.first - renderer.borderSize() - 1) * scale; ++x)
                QVERIFY2(!isInk(image.pixel(x, y)), "Hex glyphs crossed the ASCII separator");
        for (int y = qCeil(renderer.lineHeight() * scale); y < renderer.lineHeight() * 2 * scale; ++y)
            for (int x = qCeil((renderer.documentWidth() + 1) * scale); x < image.width(); ++x)
                QVERIFY2(!isInk(image.pixel(x, y)), "ASCII glyphs crossed the document boundary");
        if (length >= 16) {
            const qreal cell = QFontMetricsF(fixed, &image).horizontalAdvance(QLatin1Char('8'));
            const int lastByte = hex.first + qRound(45 * cell);
            bool ink = false;
            for (int y = qCeil(renderer.lineHeight() * scale); y < renderer.lineHeight() * 2 * scale; ++y)
                for (int x = qCeil(lastByte * scale); x < std::min(qCeil((lastByte + 2 * cell) * scale), image.width()); ++x)
                    ink |= isInk(image.pixel(x, y));
            QVERIFY2(ink, "Last hex byte was not rendered in its measured cell");
        }
    }

    void hitTestingAfterHorizontalScroll() {
        QHexView view;
        view.resize(320, 220);
        view.setDocument(QHexDocument::fromMemory<QMemoryBuffer>(QByteArray(48, 'A'), &view));
        view.setReadOnly(true); view.show(); QTest::qWait(20);
        QHexRenderer geometry(view.document(), QFontMetricsF(view.font()));
        const auto ascii = areaSpan(geometry, QHexRenderer::AsciiArea);
        view.horizontalScrollBar()->setValue(view.horizontalScrollBar()->maximum());
        const int x = ascii.first + 2 - view.horizontalScrollBar()->value();
        QVERIFY(x >= 0 && x < view.viewport()->width());
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(x, geometry.lineHeight() + geometry.lineHeight()/2));
        QCOMPARE(view.document()->cursor()->position().offset(), qint64(0));
        QCOMPARE(view.document()->read(0, 48), QByteArray(48, 'A'));
    }

    void hexCellsAndHighlightsStayAligned() {
        QHexView view;
        view.resize(1800, 220);
        view.setDocument(QHexDocument::fromMemory<QMemoryBuffer>(QByteArray(32, char(0x88)), &view));
        view.setReadOnly(true); view.show(); QTest::qWait(20);
        for (int points : {12, 18, 24}) {
            auto font = view.font(); font.setPointSize(points); view.setFont(font);
            view.resize(4000, 240); QCoreApplication::processEvents();
            QHexRenderer geometry(view.document(), QFontMetricsF(view.font(), view.viewport()));
            const auto hex = areaSpan(geometry, QHexRenderer::HexArea);
            const qreal cell = QFontMetricsF(view.font(), view.viewport()).horizontalAdvance(QLatin1Char('0'));
            for (int column : {0, 7, 15}) {
                const int x = hex.first + qFloor((column * 3 + 0.5) * cell);
                QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(x, geometry.lineHeight() * 3 / 2));
                QCOMPARE(view.document()->cursor()->position().offset(), qint64(column));
                QCOMPARE(view.document()->cursor()->currentNibble(), 1);
                QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(x + qRound(cell), geometry.lineHeight() * 3 / 2));
                QCOMPARE(view.document()->cursor()->position().offset(), qint64(column));
                QCOMPARE(view.document()->cursor()->currentNibble(), 0);
            }
            view.document()->metadata()->background(0, 0, 16, Qt::yellow);
            view.document()->cursor()->moveTo(0, 2);
            view.document()->cursor()->select(0, 4);
            const auto image = view.viewport()->grab().toImage();
            QVERIFY(!image.isNull());
            const qreal dpr = image.devicePixelRatio();
            const QColor selected = image.pixelColor(qRound((hex.first + 7 * cell) * dpr), qRound((geometry.lineHeight() + 1) * dpr));
            QCOMPARE(selected, qApp->palette().color(QPalette::Highlight));
            QCOMPARE(view.document()->read(0, 32), QByteArray(32, char(0x88)));
        }
    }

    void tinyViewportAndMultiMegabyteLastLine() {
        QHexView view;
        const QByteArray bytes(2 * 1024 * 1024 + 3, 'W');
        view.setDocument(QHexDocument::fromMemory<QMemoryBuffer>(bytes, &view));
        view.resize(90, 10); view.show(); QCoreApplication::processEvents();
        view.document()->cursor()->moveTo(bytes.size() - 1);
        view.resize(400, 250); QCoreApplication::processEvents();
        view.verticalScrollBar()->setValue(view.verticalScrollBar()->maximum());
        QVERIFY(!view.viewport()->grab().isNull());
        QHexRenderer geometry(view.document(), QFontMetricsF(view.font(), view.viewport()));
        QCOMPARE(geometry.documentLastLine(), quint64(bytes.size() / 16));
        QCOMPARE(geometry.documentLastColumn(), 3);
        QCOMPARE(view.document()->length(), qint64(bytes.size()));
    }
};

QTEST_MAIN(RenderingTest)
#include "RenderingTest.moc"

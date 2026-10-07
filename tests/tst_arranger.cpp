#include "Arranger.h"

#include <QtTest>

using Role = Arranger::Role;

namespace {

// Mono audio whose sample value equals its frame index, so rendered output
// shows exactly which source frames were played.
AudioData rampAudio(int frames)
{
    AudioData a;
    a.sampleRate = 1000;
    a.channels = 1;
    for (int i = 0; i < frames; ++i)
        a.samples.push_back(static_cast<int16_t>(i));
    return a;
}

// Indices: 0 intro, 1 var1, 2 var2, 3 fill, 4 break, 5 ending, 6 var3 (unset).
QVector<Arranger::Region> regions()
{
    return {
        {Role::Intro, 0, 100},
        {Role::Variation, 100, 200},
        {Role::Variation, 200, 300},
        {Role::Fill, 300, 350},
        {Role::Break, 350, 380},
        {Role::Ending, 400, 450},
        {Role::Variation, -1, -1},
    };
}

QVector<int> render(Arranger &arr, int frames)
{
    std::vector<int16_t> buf(size_t(frames), -1);
    arr.render(buf.data(), frames);
    return QVector<int>(buf.begin(), buf.end());
}

QVector<int> range(int from, int to) // [from, to)
{
    QVector<int> v;
    for (int i = from; i < to; ++i)
        v.push_back(i);
    return v;
}

} // namespace

class TestArranger : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void startLoopsFirstVariation();
    void armedIntroPlaysThenVariation();
    void variationSwitchWaitsForEndOfPass();
    void fillIsImmediateAndReturns();
    void breakReturnsToNewlyChosenVariation();
    void endingQueuedThenStops();
    void stopIsImmediate();
    void unsetRegionsAreIgnored();
    void crossfadeHasNoStep();

private:
    AudioData m_audio;
    Arranger m_arr;
};

void TestArranger::init()
{
    m_audio = rampAudio(500);
    m_arr = Arranger();
    m_arr.setAudio(&m_audio);
    m_arr.setCrossfadeFrames(0);
    m_arr.setRegions(regions());
}

void TestArranger::startLoopsFirstVariation()
{
    m_arr.startStop();
    QCOMPARE(render(m_arr, 250), range(100, 200) + range(100, 200) + range(100, 150));
    QVERIFY(m_arr.state().running);
    QCOMPARE(m_arr.state().playing, 1);
}

void TestArranger::armedIntroPlaysThenVariation()
{
    m_arr.press(0);
    QCOMPARE(m_arr.state().armedIntro, 0);
    m_arr.startStop();
    QCOMPARE(render(m_arr, 150), range(0, 100) + range(100, 150));
    QCOMPARE(m_arr.state().armedIntro, -1);
}

void TestArranger::variationSwitchWaitsForEndOfPass()
{
    m_arr.startStop();
    QCOMPARE(render(m_arr, 40), range(100, 140));
    m_arr.press(2);
    QCOMPARE(m_arr.state().queued, 2);
    QCOMPARE(render(m_arr, 80), range(140, 200) + range(200, 220));
    QCOMPARE(m_arr.state().playing, 2);
    QCOMPARE(m_arr.state().queued, -1);
}

void TestArranger::fillIsImmediateAndReturns()
{
    m_arr.startStop();
    render(m_arr, 30);
    m_arr.press(3);
    QCOMPARE(render(m_arr, 60), range(300, 350) + range(100, 110));
    QCOMPARE(m_arr.state().playing, 1);
}

void TestArranger::breakReturnsToNewlyChosenVariation()
{
    m_arr.startStop();
    render(m_arr, 30);
    m_arr.press(2); // queued while var1 loops
    m_arr.press(4); // break cuts in now; queued variation becomes redundant
    QCOMPARE(m_arr.state().queued, -1);
    QCOMPARE(render(m_arr, 40), range(350, 380) + range(200, 210));
    QCOMPARE(m_arr.state().playing, 2);
}

void TestArranger::endingQueuedThenStops()
{
    m_arr.startStop();
    render(m_arr, 50);
    m_arr.press(5);
    const QVector<int> out = render(m_arr, 120);
    QCOMPARE(out.mid(0, 50), range(150, 200));
    QCOMPARE(out.mid(50, 50), range(400, 450));
    QCOMPARE(out.mid(100), QVector<int>(20, 0));
    QVERIFY(!m_arr.state().running);
}

void TestArranger::stopIsImmediate()
{
    m_arr.startStop();
    render(m_arr, 10);
    m_arr.startStop();
    QVERIFY(!m_arr.state().running);
    QCOMPARE(render(m_arr, 5), QVector<int>(5, 0));
}

void TestArranger::unsetRegionsAreIgnored()
{
    m_arr.press(6); // var3 has no range
    QCOMPARE(m_arr.state().variation, 1);
    m_arr.press(3); // fill while stopped
    QVERIFY(!m_arr.state().running);
    m_arr.startStop();
    QCOMPARE(m_arr.state().playing, 1);
}

void TestArranger::crossfadeHasNoStep()
{
    // Constant-level regions make a hard cut a big step; the fade must ramp.
    AudioData a;
    a.sampleRate = 1000;
    a.channels = 1;
    a.samples.assign(500, 0);
    std::fill(a.samples.begin() + 100, a.samples.begin() + 300, int16_t(1000));  // var1 (+ tail)
    std::fill(a.samples.begin() + 300, a.samples.begin() + 350, int16_t(-1000)); // fill
    m_arr.setAudio(&a);
    m_arr.setRegions(regions());
    m_arr.setCrossfadeFrames(10);

    m_arr.startStop();
    render(m_arr, 20);
    m_arr.press(3);
    const QVector<int> out = render(m_arr, 12);
    for (int i = 1; i < out.size(); ++i)
        QVERIFY2(std::abs(out[i] - out[i - 1]) <= 200, qPrintable(QString("step at %1").arg(i)));
    QCOMPARE(out.last(), -1000);
}

QTEST_APPLESS_MAIN(TestArranger)
#include "tst_arranger.moc"

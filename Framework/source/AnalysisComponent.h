#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <functional>

// A single-producer/single-consumer tap between the real-time audio thread and
// the standalone analysis display. A full queue drops new samples rather than
// ever blocking the audio thread.
class AnalysisBuffer
{
public:
    void setSampleRate (double newSampleRate) noexcept
    {
        sampleRate.store (newSampleRate, std::memory_order_release);
    }

    double getSampleRate() const noexcept
    {
        return sampleRate.load (std::memory_order_acquire);
    }

    void push (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto channels = juce::jmin (2, buffer.getNumChannels());

        if (channels == 0)
            return;

        auto write = writePosition.load (std::memory_order_relaxed);
        const auto read = readPosition.load (std::memory_order_acquire);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto next = increment (write);

            if (next == read)
                break;

            float value = 0.0f;

            for (int channel = 0; channel < channels; ++channel)
                value += buffer.getSample (channel, sample);

            samples[write] = value / static_cast<float> (channels);
            write = next;
        }

        writePosition.store (write, std::memory_order_release);
    }

    int pop (float* destination, int maximumSamples) noexcept
    {
        auto read = readPosition.load (std::memory_order_relaxed);
        const auto write = writePosition.load (std::memory_order_acquire);
        int count = 0;

        while (read != write && count < maximumSamples)
        {
            destination[count++] = samples[read];
            read = increment (read);
        }

        readPosition.store (read, std::memory_order_release);
        return count;
    }

    void discardPending() noexcept
    {
        readPosition.store (writePosition.load (std::memory_order_acquire),
                            std::memory_order_release);
    }

private:
    static constexpr std::size_t capacity = 65536;

    static constexpr std::size_t increment (std::size_t position) noexcept
    {
        return (position + 1) % capacity;
    }

    std::array<float, capacity> samples {};
    std::atomic<std::size_t> readPosition { 0 };
    std::atomic<std::size_t> writePosition { 0 };
    std::atomic<double> sampleRate { 44100.0 };
};

class AnalysisComponent final : public juce::Component,
                                private juce::Timer
{
public:
    explicit AnalysisComponent (AnalysisBuffer& sourceIn)
        : source (sourceIn),
          tabs (juce::TabbedButtonBar::TabsAtTop),
          scopeView (*this, Plot::scope),
          spectrumView (*this, Plot::spectrum),
          sonogramView (*this, Plot::sonogram),
          sonogram (juce::Image::RGB, 768, 512, true)
    {
        spectrum.fill (minimumDecibels);
        targetSpectrum.fill (minimumDecibels);

        tabs.setTabBarDepth (28);
        tabs.setColour (juce::TabbedComponent::backgroundColourId, gutterColour);
        tabs.setColour (juce::TabbedComponent::outlineColourId, gutterColour);
        tabs.getTabbedButtonBar().setColour (juce::TabbedButtonBar::tabOutlineColourId,
                                             gutterColour.darker (0.15f));
        tabs.getTabbedButtonBar().setColour (juce::TabbedButtonBar::frontOutlineColourId,
                                             gutterColour.darker (0.15f));
        tabs.addTab ("Oscilloscope", gutterColour, &scopeView, false);
        tabs.addTab ("Spectrum", gutterColour, &spectrumView, false);
        tabs.addTab ("Sonogram", gutterColour, &sonogramView, false);

        detachButton.onClick = [this]
        {
            if (detachCallback)
                detachCallback();
        };

        addAndMakeVisible (tabs);
        addAndMakeVisible (detachButton);
        startTimerHz (60);
    }

    void setDetachCallback (std::function<void()> callback)
    {
        detachCallback = std::move (callback);
    }

    void setDetached (bool shouldBeDetached)
    {
        detached = shouldBeDetached;
        detachButton.setButtonText (detached ? "Reattach" : "Detach");
    }

    void paint (juce::Graphics& graphics) override
    {
        graphics.fillAll (panelColour);
    }

    void resized() override
    {
        tabs.setBounds (getLocalBounds());
        detachButton.setBounds (getWidth() - 84, 2, 80, tabs.getTabBarDepth() - 4);
        detachButton.toFront (false);
    }

private:
    enum class Plot { scope, spectrum, sonogram };

    class PlotView final : public juce::Component
    {
    public:
        PlotView (AnalysisComponent& ownerIn, Plot plotIn)
            : owner (ownerIn), plot (plotIn)
        {
            setMouseCursor (juce::MouseCursor::CrosshairCursor);
        }

        void paint (juce::Graphics& graphics) override
        {
            owner.paintPlot (graphics, getLocalBounds(), plot, mousePosition, mouseIsInside);
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu())
                owner.showContextMenu (plot, this);
        }

        void mouseMove (const juce::MouseEvent& event) override
        {
            mousePosition = event.position;
            repaint();
        }

        void mouseEnter (const juce::MouseEvent& event) override
        {
            mouseIsInside = true;
            mousePosition = event.position;
            repaint();
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            mouseIsInside = false;
            repaint();
        }

    private:
        AnalysisComponent& owner;
        Plot plot;
        juce::Point<float> mousePosition;
        bool mouseIsInside = false;
    };

    static constexpr int maximumFftSize = 32768;
    static constexpr int spectrumHistoryDepth = 6;
    static constexpr int scopeCapacity = 16384;

    enum Command
    {
        toggleGrid = 1,
        togglePause = 2,
        frequencyLinear = 10,
        frequencyLogarithmic = 11,
        fft512 = 20,
        fft1024 = 21,
        fft2048 = 22,
        fft4096 = 23,
        fft8192 = 24,
        fft16384 = 25,
        fft32768 = 26,
        range60 = 30,
        range90 = 31,
        range120 = 32,
        scope5ms = 40,
        scope10ms = 41,
        scope20ms = 42,
        scope50ms = 43,
        scope100ms = 44,
        toggleTrigger = 50,
        refresh30Hz = 60,
        refresh60Hz = 61,
        smoothingOff = 70,
        smoothingFast = 71,
        smoothingMedium = 72,
        smoothingSlow = 73
    };

    void timerCallback() override
    {
        if (! isShowing() || paused)
        {
            source.discardPending();
            fftSamplesAvailable = 0;
            return;
        }

        std::array<float, 4096> incoming {};
        const auto count = source.pop (incoming.data(), static_cast<int> (incoming.size()));
        const auto activeTab = tabs.getCurrentTabIndex();
        const auto needsFft = activeTab == 1 || activeTab == 2;

        for (int i = 0; i < count; ++i)
        {
            const auto sample = incoming[static_cast<std::size_t> (i)];
            scopeSamples[scopeWritePosition] = sample;
            scopeWritePosition = (scopeWritePosition + 1) % scopeCapacity;
            fftHistory[fftWritePosition] = sample;
            fftWritePosition = (fftWritePosition + 1) % maximumFftSize;
            fftSamplesAvailable = juce::jmin (maximumFftSize, fftSamplesAvailable + 1);
        }

        if (needsFft && count > 0 && fftSamplesAvailable >= fftSize)
        {
            const auto fftInterval = fftSize >= 16384 ? 2 : 1;

            if (++fftFrameCounter >= fftInterval)
            {
                updateSpectrum();
                fftFrameCounter = 0;
            }

            smoothSpectrum();
            captureSpectrumHistory();

            if (activeTab == 2)
                updateSonogram();
        }

        if (count > 0 || needsFft)
        {
            if (activeTab == 0)
                scopeView.repaint();
            else if (activeTab == 1)
                spectrumView.repaint();
            else
                sonogramView.repaint();
        }
    }

    static void transform (std::complex<float>* values, int size)
    {
        for (int i = 1, j = 0; i < size; ++i)
        {
            auto bit = size >> 1;

            for (; (j & bit) != 0; bit >>= 1)
                j ^= bit;

            j ^= bit;

            if (i < j)
                std::swap (values[i], values[j]);
        }

        constexpr auto pi = 3.14159265358979323846f;

        for (int length = 2; length <= size; length <<= 1)
        {
            const auto angle = -2.0f * pi / static_cast<float> (length);
            const std::complex<float> step (std::cos (angle), std::sin (angle));

            for (int offset = 0; offset < size; offset += length)
            {
                std::complex<float> phase (1.0f, 0.0f);

                for (int i = 0; i < length / 2; ++i)
                {
                    const auto even = values[offset + i];
                    const auto odd = values[offset + i + length / 2] * phase;
                    values[offset + i] = even + odd;
                    values[offset + i + length / 2] = even - odd;
                    phase *= step;
                }
            }
        }
    }

    void updateSpectrum()
    {
        constexpr auto pi = 3.14159265358979323846f;
        const auto start = (fftWritePosition + maximumFftSize - static_cast<std::size_t> (fftSize))
                         % maximumFftSize;

        for (int i = 0; i < fftSize; ++i)
        {
            const auto window = 0.5f - 0.5f * std::cos (2.0f * pi * static_cast<float> (i)
                                                       / static_cast<float> (fftSize - 1));
            const auto index = (start + static_cast<std::size_t> (i)) % maximumFftSize;
            fftWork[static_cast<std::size_t> (i)] = fftHistory[index] * window;
        }

        transform (fftWork.data(), fftSize);
        spectrumBins = fftSize / 2;

        for (int bin = 0; bin < spectrumBins; ++bin)
        {
            const auto magnitude = std::abs (fftWork[static_cast<std::size_t> (bin)])
                                 / static_cast<float> (fftSize);
            const auto target = juce::jlimit (
                minimumDecibels, 0.0f,
                20.0f * std::log10 (juce::jmax (magnitude, 0.000001f)));
            targetSpectrum[static_cast<std::size_t> (bin)] = target;
        }

    }

    void smoothSpectrum()
    {
        for (int bin = 0; bin < spectrumBins; ++bin)
        {
            auto& displayed = spectrum[static_cast<std::size_t> (bin)];
            displayed += (targetSpectrum[static_cast<std::size_t> (bin)] - displayed)
                       * smoothingAmount;
        }
    }

    void captureSpectrumHistory()
    {
        if (++historyFrameCounter < 4)
            return;

        historyFrameCounter = 0;
        std::copy_n (spectrum.begin(), spectrumBins,
                     spectrumHistory[static_cast<std::size_t> (spectrumHistoryWrite)].begin());
        spectrumHistoryWrite = (spectrumHistoryWrite + 1) % spectrumHistoryDepth;
        spectrumHistoryCount = juce::jmin (spectrumHistoryDepth, spectrumHistoryCount + 1);
    }

    void updateSonogram()
    {
        juce::Image::BitmapData column (sonogram,
                                        sonogramWriteColumn, 0,
                                        1, sonogram.getHeight(),
                                        juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < sonogram.getHeight(); ++y)
        {
            const auto position = 1.0f - static_cast<float> (y)
                                               / static_cast<float> (sonogram.getHeight() - 1);
            const auto bin = binForPosition (position);
            const auto level = juce::jmap (spectrum[bin], minimumDecibels, 0.0f, 0.0f, 1.0f);
            const auto colour = juce::Colour::fromHSV (0.68f - 0.68f * level,
                                                       0.9f,
                                                       juce::jlimit (0.0f, 1.0f, level),
                                                       1.0f);
            column.setPixelColour (0, y, colour);
        }

        sonogramWriteColumn = (sonogramWriteColumn + 1) % sonogram.getWidth();
    }

    void paintPlot (juce::Graphics& graphics,
                    juce::Rectangle<int> area,
                    Plot plot,
                    juce::Point<float> mousePosition,
                    bool mouseIsInside) const
    {
        graphics.setColour (plotColour);
        graphics.fillRect (area);

        auto bounds = area.toFloat().withTrimmedLeft (52.0f)
                                    .withTrimmedRight (10.0f)
                                    .withTrimmedTop (10.0f)
                                    .withTrimmedBottom (24.0f);

        if (plot == Plot::scope)
        {
            if (showGrid)
                paintGrid (graphics, bounds, plot);
            paintScope (graphics, bounds);
        }
        else if (plot == Plot::spectrum)
        {
            if (showGrid)
                paintGrid (graphics, bounds, plot);
            paintSpectrum (graphics, bounds);
        }
        else
        {
            paintSonogram (graphics, bounds);

            if (showGrid)
                paintGrid (graphics, bounds, plot);
        }

        if (mouseIsInside && bounds.contains (mousePosition))
            paintHover (graphics, bounds, plot, mousePosition);
    }

    void paintGrid (juce::Graphics& graphics, juce::Rectangle<float> bounds, Plot plot) const
    {
        graphics.setFont (10.0f);
        const auto gridColour = juce::Colours::white.withAlpha (0.13f);
        const auto labelColour = juce::Colours::white.withAlpha (0.65f);

        if (plot == Plot::scope)
        {
            for (int division = 0; division <= 5; ++division)
            {
                const auto proportion = static_cast<float> (division) / 5.0f;
                const auto x = bounds.getX() + proportion * bounds.getWidth();
                graphics.setColour (gridColour);
                graphics.drawVerticalLine (juce::roundToInt (x), bounds.getY(), bounds.getBottom());
                graphics.setColour (labelColour);
                graphics.drawText (juce::String (scopeMilliseconds * proportion, 1) + " ms",
                                   juce::Rectangle<float> (x - 28.0f, bounds.getBottom() + 3.0f, 56.0f, 16.0f),
                                   juce::Justification::centred, false);
            }

            for (int division = 0; division <= 4; ++division)
            {
                const auto proportion = static_cast<float> (division) / 4.0f;
                const auto amplitude = 1.0f - 2.0f * proportion;
                const auto y = bounds.getY() + proportion * bounds.getHeight();
                graphics.setColour (division == 2 ? juce::Colours::white.withAlpha (0.25f) : gridColour);
                graphics.drawHorizontalLine (juce::roundToInt (y), bounds.getX(), bounds.getRight());
                graphics.setColour (labelColour);
                graphics.drawText (juce::String (amplitude, 1),
                                   juce::Rectangle<float> (2.0f, y - 8.0f, 45.0f, 16.0f),
                                   juce::Justification::centredRight, false);
            }

            return;
        }

        if (plot == Plot::spectrum)
        {
            for (int division = 0; division <= 6; ++division)
            {
                const auto proportion = static_cast<float> (division) / 6.0f;
                const auto decibels = -proportion * -minimumDecibels;
                const auto y = bounds.getY() + proportion * bounds.getHeight();
                graphics.setColour (gridColour);
                graphics.drawHorizontalLine (juce::roundToInt (y), bounds.getX(), bounds.getRight());
                graphics.setColour (labelColour);
                graphics.drawText (juce::String (decibels, 0) + " dB",
                                   juce::Rectangle<float> (2.0f, y - 8.0f, 45.0f, 16.0f),
                                   juce::Justification::centredRight, false);
            }

            paintFrequencyGrid (graphics, bounds, false, gridColour, labelColour);
            return;
        }

        const auto historySeconds = static_cast<float> (sonogram.getWidth())
                                  / static_cast<float> (refreshRate);

        for (int division = 0; division <= 4; ++division)
        {
            const auto proportion = static_cast<float> (division) / 4.0f;
            const auto x = bounds.getX() + proportion * bounds.getWidth();
            const auto secondsAgo = (1.0f - proportion) * historySeconds;
            graphics.setColour (gridColour);
            graphics.drawVerticalLine (juce::roundToInt (x), bounds.getY(), bounds.getBottom());
            graphics.setColour (labelColour);
            graphics.drawText ("-" + juce::String (secondsAgo, 1) + " s",
                               juce::Rectangle<float> (x - 28.0f, bounds.getBottom() + 3.0f, 56.0f, 16.0f),
                               juce::Justification::centred, false);
        }

        paintFrequencyGrid (graphics, bounds, true, gridColour, labelColour);
    }

    void paintFrequencyGrid (juce::Graphics& graphics,
                             juce::Rectangle<float> bounds,
                             bool verticalAxis,
                             juce::Colour gridColour,
                             juce::Colour labelColour) const
    {
        const auto nyquist = static_cast<float> (source.getSampleRate() * 0.5);

        const auto paintTick = [&] (float frequency)
        {
            if (frequency < 0.0f || frequency > nyquist)
                return;

            const auto position = positionForFrequency (frequency);
            graphics.setColour (gridColour);

            if (verticalAxis)
            {
                const auto y = bounds.getBottom() - position * bounds.getHeight();
                graphics.drawHorizontalLine (juce::roundToInt (y), bounds.getX(), bounds.getRight());
                graphics.setColour (labelColour);
                graphics.drawText (formatFrequency (frequency),
                                   juce::Rectangle<float> (2.0f, y - 8.0f, 45.0f, 16.0f),
                                   juce::Justification::centredRight, false);
            }
            else
            {
                const auto x = bounds.getX() + position * bounds.getWidth();
                graphics.drawVerticalLine (juce::roundToInt (x), bounds.getY(), bounds.getBottom());
                graphics.setColour (labelColour);
                graphics.drawText (formatFrequency (frequency),
                                   juce::Rectangle<float> (x - 28.0f, bounds.getBottom() + 3.0f, 56.0f, 16.0f),
                                   juce::Justification::centred, false);
            }
        };

        if (logarithmicFrequency)
        {
            constexpr std::array<float, 6> ticks { 20.0f, 100.0f, 500.0f,
                                                   2000.0f, 10000.0f, 20000.0f };

            for (const auto frequency : ticks)
                paintTick (frequency);
        }
        else
        {
            for (int division = 0; division <= 4; ++division)
                paintTick (nyquist * static_cast<float> (division) / 4.0f);
        }
    }

    void paintSonogram (juce::Graphics& graphics, juce::Rectangle<float> bounds) const
    {
        const auto destination = bounds.toNearestInt();
        const auto firstSourceWidth = sonogram.getWidth() - sonogramWriteColumn;
        const auto firstDestinationWidth = juce::roundToInt (
            static_cast<float> (destination.getWidth()) * firstSourceWidth
            / static_cast<float> (sonogram.getWidth()));

        graphics.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);

        if (firstSourceWidth > 0 && firstDestinationWidth > 0)
            graphics.drawImage (sonogram,
                                destination.getX(), destination.getY(),
                                firstDestinationWidth, destination.getHeight(),
                                sonogramWriteColumn, 0,
                                firstSourceWidth, sonogram.getHeight());

        if (sonogramWriteColumn > 0 && firstDestinationWidth < destination.getWidth())
            graphics.drawImage (sonogram,
                                destination.getX() + firstDestinationWidth, destination.getY(),
                                destination.getWidth() - firstDestinationWidth, destination.getHeight(),
                                0, 0, sonogramWriteColumn, sonogram.getHeight());
    }

    void paintHover (juce::Graphics& graphics,
                     juce::Rectangle<float> bounds,
                     Plot plot,
                     juce::Point<float> mouse) const
    {
        const auto xPosition = juce::jlimit (0.0f, 1.0f,
                                             (mouse.x - bounds.getX()) / bounds.getWidth());
        const auto yPosition = juce::jlimit (0.0f, 1.0f,
                                             (mouse.y - bounds.getY()) / bounds.getHeight());
        juce::String text;
        auto markerY = mouse.y;

        if (plot == Plot::scope)
        {
            const auto time = xPosition * static_cast<float> (scopeMilliseconds);
            const auto amplitude = 1.0f - 2.0f * yPosition;
            text = juce::String (time, 2) + " ms, " + juce::String (amplitude, 3);
        }
        else if (plot == Plot::spectrum)
        {
            const auto frequency = frequencyForPosition (xPosition);
            const auto decibels = spectrum[binForPosition (xPosition)];
            markerY = juce::jmap (decibels, minimumDecibels, 0.0f,
                                  bounds.getBottom(), bounds.getY());
            text = formatFrequency (frequency) + ", " + juce::String (decibels, 1) + " dB";
        }
        else
        {
            const auto historySeconds = static_cast<float> (sonogram.getWidth())
                                      / static_cast<float> (refreshRate);
            const auto secondsAgo = (1.0f - xPosition) * historySeconds;
            const auto frequency = frequencyForPosition (1.0f - yPosition);
            text = "-" + juce::String (secondsAgo, 2) + " s, " + formatFrequency (frequency);
        }

        graphics.setColour (juce::Colours::white.withAlpha (0.38f));
        graphics.drawVerticalLine (juce::roundToInt (mouse.x), bounds.getY(), bounds.getBottom());
        graphics.drawHorizontalLine (juce::roundToInt (markerY), bounds.getX(), bounds.getRight());
        graphics.fillEllipse (mouse.x - 2.5f, markerY - 2.5f, 5.0f, 5.0f);

        graphics.setFont (12.0f);
        const auto textWidth = 170.0f;
        auto tooltip = juce::Rectangle<float> (mouse.x + 10.0f, mouse.y - 28.0f,
                                               textWidth, 22.0f);

        if (tooltip.getRight() > bounds.getRight())
            tooltip.setX (mouse.x - textWidth - 10.0f);

        if (tooltip.getY() < bounds.getY())
            tooltip.setY (mouse.y + 10.0f);

        graphics.setColour (juce::Colour (0xe6202227));
        graphics.fillRoundedRectangle (tooltip, 3.0f);
        graphics.setColour (juce::Colours::white);
        graphics.drawText (text, tooltip, juce::Justification::centred, false);
    }

    void paintScope (juce::Graphics& graphics, juce::Rectangle<float> bounds) const
    {
        juce::Path path;
        const auto sampleRate = juce::jmax (1.0, source.getSampleRate());
        const auto requestedSamples = juce::roundToInt (sampleRate * scopeMilliseconds / 1000.0);
        const auto displaySamples = juce::jlimit (32, scopeCapacity, requestedSamples);
        auto start = (scopeWritePosition + scopeCapacity - static_cast<std::size_t> (displaySamples))
                   % scopeCapacity;

        if (triggerScope && displaySamples < scopeCapacity)
        {
            const auto searchSamples = juce::jmin (displaySamples / 4, scopeCapacity - displaySamples);
            const auto searchStart = (start + scopeCapacity - static_cast<std::size_t> (searchSamples))
                                   % scopeCapacity;
            start = (searchStart + static_cast<std::size_t> (searchSamples)) % scopeCapacity;

            for (int i = 1; i < searchSamples; ++i)
            {
                const auto previous = scopeSamples[(searchStart + static_cast<std::size_t> (i - 1)) % scopeCapacity];
                const auto current = scopeSamples[(searchStart + static_cast<std::size_t> (i)) % scopeCapacity];

                if (previous < 0.0f && current >= 0.0f)
                {
                    start = (searchStart + static_cast<std::size_t> (i)) % scopeCapacity;
                    break;
                }
            }
        }

        const auto pointCount = juce::jlimit (2, 1200, static_cast<int> (bounds.getWidth()));

        for (int point = 0; point < pointCount; ++point)
        {
            const auto offset = point * displaySamples / juce::jmax (1, pointCount - 1);
            const auto index = (start + static_cast<std::size_t> (offset)) % scopeCapacity;
            const auto y = bounds.getCentreY()
                         - juce::jlimit (-1.0f, 1.0f, scopeSamples[index]) * bounds.getHeight() * 0.46f;
            const auto px = juce::jmap (static_cast<float> (point),
                                        0.0f, static_cast<float> (pointCount - 1),
                                        bounds.getX(), bounds.getRight());

            if (point == 0)
                path.startNewSubPath (px, y);
            else
                path.lineTo (px, y);
        }

        graphics.setColour (juce::Colour (0xff47d7ff));
        graphics.strokePath (path, juce::PathStrokeType (1.5f));
    }

    void paintSpectrum (juce::Graphics& graphics, juce::Rectangle<float> bounds) const
    {
        for (int history = 0; history < spectrumHistoryCount; ++history)
        {
            const auto index = (spectrumHistoryWrite - spectrumHistoryCount + history
                                + spectrumHistoryDepth) % spectrumHistoryDepth;
            const auto alpha = juce::jmap (static_cast<float> (history),
                                           0.0f,
                                           static_cast<float> (juce::jmax (1, spectrumHistoryCount - 1)),
                                           0.035f, 0.24f);
            paintSpectrumTrace (graphics, bounds,
                                spectrumHistory[static_cast<std::size_t> (index)].data(),
                                juce::Colour (0xffffc857).withAlpha (alpha), 1.0f);
        }

        paintSpectrumTrace (graphics, bounds, spectrum.data(),
                            juce::Colour (0xffffc857), 1.5f);
    }

    void paintSpectrumTrace (juce::Graphics& graphics,
                             juce::Rectangle<float> bounds,
                             const float* values,
                             juce::Colour colour,
                             float thickness) const
    {
        juce::Path path;

        const auto pointCount = juce::jlimit (2, 1200, static_cast<int> (bounds.getWidth()));

        for (int point = 0; point < pointCount; ++point)
        {
            const auto position = static_cast<float> (point) / static_cast<float> (pointCount - 1);
            const auto level = juce::jmap (values[binForPosition (position)],
                                           minimumDecibels, 0.0f, 0.0f, 1.0f);
            const auto px = juce::jmap (static_cast<float> (point),
                                        0.0f, static_cast<float> (pointCount - 1),
                                        bounds.getX(), bounds.getRight());
            const auto py = bounds.getBottom()
                          - juce::jlimit (0.0f, 1.0f, level) * bounds.getHeight();

            if (point == 0)
                path.startNewSubPath (px, py);
            else
                path.lineTo (px, py);
        }

        graphics.setColour (colour);
        graphics.strokePath (path, juce::PathStrokeType (thickness));
    }

    float frequencyForPosition (float position) const
    {
        const auto sampleRate = static_cast<float> (juce::jmax (1.0, source.getSampleRate()));
        const auto nyquist = sampleRate * 0.5f;
        float frequency = position * nyquist;

        if (logarithmicFrequency)
        {
            const auto minimumFrequency = juce::jmin (20.0f, nyquist);
            frequency = minimumFrequency
                      * std::pow (juce::jmax (1.0f, nyquist / minimumFrequency), position);
        }

        return frequency;
    }

    float positionForFrequency (float frequency) const
    {
        const auto nyquist = static_cast<float> (juce::jmax (1.0, source.getSampleRate() * 0.5));

        if (! logarithmicFrequency)
            return juce::jlimit (0.0f, 1.0f, frequency / nyquist);

        const auto minimumFrequency = juce::jmin (20.0f, nyquist);
        return juce::jlimit (0.0f, 1.0f,
                            std::log (juce::jmax (frequency, minimumFrequency) / minimumFrequency)
                            / std::log (juce::jmax (1.0f, nyquist / minimumFrequency)));
    }

    static juce::String formatFrequency (float frequency)
    {
        if (frequency >= 1000.0f)
            return juce::String (frequency / 1000.0f, frequency < 10000.0f ? 1 : 0) + " kHz";

        return juce::String (frequency, 0) + " Hz";
    }

    std::size_t binForPosition (float position) const
    {
        const auto sampleRate = static_cast<float> (juce::jmax (1.0, source.getSampleRate()));
        const auto frequency = frequencyForPosition (position);

        const auto bin = juce::roundToInt (frequency * static_cast<float> (fftSize) / sampleRate);
        return static_cast<std::size_t> (juce::jlimit (0, juce::jmax (0, spectrumBins - 1), bin));
    }

    void showContextMenu (Plot plot, juce::Component* target)
    {
        juce::PopupMenu menu;
        menu.addItem (toggleGrid, "Grid lines", true, showGrid);
        menu.addItem (togglePause, "Pause", true, paused);

        juce::PopupMenu refreshMenu;
        refreshMenu.addItem (refresh30Hz, "30 Hz", true, refreshRate == 30);
        refreshMenu.addItem (refresh60Hz, "60 Hz", true, refreshRate == 60);
        menu.addSubMenu ("Refresh rate", refreshMenu);

        if (plot == Plot::scope)
        {
            juce::PopupMenu timeMenu;
            timeMenu.addItem (scope5ms, "5 ms", true, scopeMilliseconds == 5);
            timeMenu.addItem (scope10ms, "10 ms", true, scopeMilliseconds == 10);
            timeMenu.addItem (scope20ms, "20 ms", true, scopeMilliseconds == 20);
            timeMenu.addItem (scope50ms, "50 ms", true, scopeMilliseconds == 50);
            timeMenu.addItem (scope100ms, "100 ms", true, scopeMilliseconds == 100);
            menu.addSubMenu ("Time span", timeMenu);
            menu.addItem (toggleTrigger, "Zero-crossing trigger", true, triggerScope);
        }
        else
        {
            juce::PopupMenu scaleMenu;
            scaleMenu.addItem (frequencyLinear, "Linear", true, ! logarithmicFrequency);
            scaleMenu.addItem (frequencyLogarithmic, "Logarithmic", true, logarithmicFrequency);
            menu.addSubMenu ("Frequency scale", scaleMenu);

            juce::PopupMenu fftMenu;
            fftMenu.addItem (fft512, "512 samples", true, fftSize == 512);
            fftMenu.addItem (fft1024, "1024 samples", true, fftSize == 1024);
            fftMenu.addItem (fft2048, "2048 samples", true, fftSize == 2048);
            fftMenu.addItem (fft4096, "4096 samples", true, fftSize == 4096);
            fftMenu.addItem (fft8192, "8192 samples", true, fftSize == 8192);
            fftMenu.addItem (fft16384, "16384 samples", true, fftSize == 16384);
            fftMenu.addItem (fft32768, "32768 samples", true, fftSize == 32768);
            menu.addSubMenu ("FFT size", fftMenu);

            juce::PopupMenu rangeMenu;
            rangeMenu.addItem (range60, "60 dB", true, minimumDecibels == -60.0f);
            rangeMenu.addItem (range90, "90 dB", true, minimumDecibels == -90.0f);
            rangeMenu.addItem (range120, "120 dB", true, minimumDecibels == -120.0f);
            menu.addSubMenu ("Display range", rangeMenu);

            juce::PopupMenu smoothingMenu;
            smoothingMenu.addItem (smoothingOff, "Off", true, smoothingAmount == 1.0f);
            smoothingMenu.addItem (smoothingFast, "Fast", true, smoothingAmount == 0.55f);
            smoothingMenu.addItem (smoothingMedium, "Medium", true, smoothingAmount == 0.28f);
            smoothingMenu.addItem (smoothingSlow, "Slow", true, smoothingAmount == 0.12f);
            menu.addSubMenu ("Smoothing", smoothingMenu);
        }

        juce::Component::SafePointer<AnalysisComponent> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target)
                                                      .withMousePosition(),
                            [safeThis] (int result)
                            {
                                if (safeThis != nullptr)
                                    safeThis->handleMenuCommand (result);
                            });
    }

    void handleMenuCommand (int command)
    {
        if (command == 0)
            return;

        switch (command)
        {
            case toggleGrid:             showGrid = ! showGrid; break;
            case togglePause:            paused = ! paused; break;
            case frequencyLinear:        logarithmicFrequency = false; clearSonogram(); break;
            case frequencyLogarithmic:   logarithmicFrequency = true; clearSonogram(); break;
            case fft512:                 setFftSize (512); break;
            case fft1024:                setFftSize (1024); break;
            case fft2048:                setFftSize (2048); break;
            case fft4096:                setFftSize (4096); break;
            case fft8192:                setFftSize (8192); break;
            case fft16384:               setFftSize (16384); break;
            case fft32768:               setFftSize (32768); break;
            case range60:                setDisplayRange (-60.0f); break;
            case range90:                setDisplayRange (-90.0f); break;
            case range120:               setDisplayRange (-120.0f); break;
            case scope5ms:               scopeMilliseconds = 5; break;
            case scope10ms:              scopeMilliseconds = 10; break;
            case scope20ms:              scopeMilliseconds = 20; break;
            case scope50ms:              scopeMilliseconds = 50; break;
            case scope100ms:             scopeMilliseconds = 100; break;
            case toggleTrigger:          triggerScope = ! triggerScope; break;
            case refresh30Hz:            setRefreshRate (30); break;
            case refresh60Hz:            setRefreshRate (60); break;
            case smoothingOff:           smoothingAmount = 1.0f; break;
            case smoothingFast:          smoothingAmount = 0.55f; break;
            case smoothingMedium:        smoothingAmount = 0.28f; break;
            case smoothingSlow:          smoothingAmount = 0.12f; break;
            default:                     break;
        }

        scopeView.repaint();
        spectrumView.repaint();
        sonogramView.repaint();
    }

    void setFftSize (int newSize)
    {
        fftSize = newSize;
        spectrumBins = fftSize / 2;
        spectrum.fill (minimumDecibels);
        targetSpectrum.fill (minimumDecibels);
        clearSpectrumHistory();
        clearSonogram();
    }

    void setDisplayRange (float newMinimum)
    {
        minimumDecibels = newMinimum;
        spectrum.fill (minimumDecibels);
        targetSpectrum.fill (minimumDecibels);
        clearSpectrumHistory();
        clearSonogram();
    }

    void setRefreshRate (int newRate)
    {
        refreshRate = newRate;
        startTimerHz (refreshRate);
    }

    void clearSonogram()
    {
        sonogram.clear (sonogram.getBounds(), plotColour);
        sonogramWriteColumn = 0;
    }

    void clearSpectrumHistory()
    {
        spectrumHistoryWrite = 0;
        spectrumHistoryCount = 0;
        historyFrameCounter = 0;
    }

    static inline const juce::Colour panelColour { 0xff24272d };
    static inline const juce::Colour gutterColour { 0xff747474 };
    static inline const juce::Colour plotColour { 0xff17191d };

    AnalysisBuffer& source;
    juce::TabbedComponent tabs;
    PlotView scopeView;
    PlotView spectrumView;
    PlotView sonogramView;
    juce::TextButton detachButton { "Detach" };
    std::function<void()> detachCallback;
    bool detached = false;
    bool showGrid = true;
    bool paused = false;
    bool logarithmicFrequency = true;
    bool triggerScope = true;
    int fftSize = 2048;
    int spectrumBins = fftSize / 2;
    int fftSamplesAvailable = 0;
    int fftFrameCounter = 0;
    int historyFrameCounter = 0;
    int spectrumHistoryWrite = 0;
    int spectrumHistoryCount = 0;
    int scopeMilliseconds = 20;
    int refreshRate = 60;
    float minimumDecibels = -90.0f;
    float smoothingAmount = 0.28f;
    std::array<float, scopeCapacity> scopeSamples {};
    std::array<float, maximumFftSize> fftHistory {};
    std::array<std::complex<float>, maximumFftSize> fftWork {};
    std::array<float, maximumFftSize / 2> spectrum {};
    std::array<float, maximumFftSize / 2> targetSpectrum {};
    std::array<std::array<float, maximumFftSize / 2>, spectrumHistoryDepth> spectrumHistory {};
    std::size_t scopeWritePosition = 0;
    std::size_t fftWritePosition = 0;
    int sonogramWriteColumn = 0;
    juce::Image sonogram;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisComponent)
};

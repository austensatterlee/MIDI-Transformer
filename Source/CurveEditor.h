#pragma once
#include <JuceHeader.h>

namespace aas
{
    template <typename T>
    class CurveEditor : public juce::Component
    {
        using PointType = juce::Point<T>;
    public:
        explicit CurveEditor(float minX = 0.0f, float maxX = 1.0f, float minY = 0.0f, float maxY = 1.0f):
            minX (minX),
            maxX (maxX),
            minY (minY),
            maxY (maxY)
        {
            points.emplace_back (PointType{0.0f, 0.0f});
            points.emplace_back (PointType{0.5f, 0.5f});
            points.emplace_back (PointType{1.0f, 1.0f});
        }

        void paint(Graphics& g) override;
        void mouseDown(const MouseEvent& event) override;
        void mouseDrag(const MouseEvent& event) override;
        void mouseUp(const MouseEvent& event) override;
        void mouseDoubleClick(const MouseEvent& event) override;
        void mouseMove(const MouseEvent& event) override;
        void resized() override;

        void addPoint(const PointType& p);

        /**
         * Map an input value onto the curve
         */
        float compute(T input);

        PointType transformPointToScreenSpace(const PointType& p) const;
        PointType transformPointFromScreenSpace(const PointType& p) const;
        const float pointSize = 10.0f;
        float minX;
        float maxX;
        float minY;
        float maxY;
    private:
        std::vector<PointType> points;
        PointType* selectedPoint = nullptr;
        juce::AffineTransform screenSpaceTransform;
    };

    template <typename T>
    void CurveEditor<T>::paint(Graphics& g)
    {
        g.setColour(Colours::black);
        g.fillRect(0, 0, getWidth(), getHeight());

        Path curve;
        for (size_t i = 0; i < points.size(); i++)
        {
            const auto transformedPoint = transformPointToScreenSpace(points[i]);
            if (i == 0)
                curve.startNewSubPath(transformedPoint);
            else
                curve.lineTo(transformedPoint);
            if (selectedPoint == &points[i]) {
                g.setColour(Colours::red);
                g.fillEllipse(transformedPoint.x - pointSize * 0.5f,
                              transformedPoint.y - pointSize * 0.5f,
                              pointSize, pointSize);
            }
            else {
                g.setColour(Colours::goldenrod);
                g.drawEllipse(transformedPoint.x - pointSize * 0.5f,
                              transformedPoint.y - pointSize * 0.5f,
                              pointSize, pointSize, 3.0f);
            }
        }

        g.setColour(Colours::whitesmoke);
        g.strokePath(curve, PathStrokeType(1.0f));

        // TODO
        const PointType screenSpaceMousePt = getMouseXYRelative().toFloat();
        if (contains(getMouseXYRelative())) {
            const auto modelSpaceMousePt = transformPointFromScreenSpace(screenSpaceMousePt);
            const auto yValue = PointType(modelSpaceMousePt.x, compute(modelSpaceMousePt.x));
            const auto yValueScreenSpace = transformPointToScreenSpace(yValue);
            g.setColour(Colours::red);

            g.drawVerticalLine(static_cast<int> (screenSpaceMousePt.x), jmin(yValueScreenSpace.y, screenSpaceMousePt.y), jmax(yValueScreenSpace.y, screenSpaceMousePt.y));
        }
    }

    template <typename T>
    void CurveEditor<T>::mouseDown(const MouseEvent& event)
    {
        jassert(!points.empty());

        const auto mousePt = event.getPosition().toFloat();
        PointType* closestPoint = nullptr;
        float closestPointDist = 0;
        const float distanceThreshold = pointSize * 2.0f;
        for (auto& p : points)
        {
            const auto dist = transformPointToScreenSpace(p).getDistanceFrom(mousePt);
            if (dist < closestPointDist || closestPoint == nullptr)
            {
                closestPoint = &p;
                closestPointDist = dist;
            }
        }
        if (closestPointDist < distanceThreshold)
            selectedPoint = closestPoint;
        else
            selectedPoint = nullptr;

        repaint();
    }

    template <typename T>
    void CurveEditor<T>::mouseDrag(const MouseEvent& event)
    {
        if (selectedPoint)
        {
            // TODO: Sort points
            const PointType mousePt = event.getPosition().toFloat();
            const PointType modelSpaceMousePt = transformPointFromScreenSpace(mousePt);

            // Adjust selected point within the X and Y boundaries
            *selectedPoint = *selectedPoint + 0.9f * (modelSpaceMousePt - *selectedPoint);
            selectedPoint->setX(jlimit(minX, maxX, selectedPoint->getX()));
            selectedPoint->setY(jlimit(minY, maxY, selectedPoint->getY()));

            // Lock the X position of the first and last points
            if (selectedPoint == &points.front())
            {
                selectedPoint->setX(minX);
            }
            else if (selectedPoint == &points.back())
            {
                selectedPoint->setX(maxX);
            }

            // Allow points to be seamlessly dragged passed each other
            if (selectedPoint > &points.front() && selectedPoint->x < (selectedPoint - 1)->x)
            {
                std::swap(*selectedPoint, *(selectedPoint - 1));
                selectedPoint = selectedPoint - 1;
            }
            else if (selectedPoint < &points.back() && selectedPoint->x >(selectedPoint + 1)->x)
            {
                std::swap(*selectedPoint, *(selectedPoint + 1));
                selectedPoint = selectedPoint + 1;
            }
            repaint();
        }
    }

    template <typename T>
    void CurveEditor<T>::mouseUp(const MouseEvent& event)
    {
        selectedPoint = nullptr;
    }

    template <typename T>
    void CurveEditor<T>::mouseDoubleClick(const MouseEvent& event)
    {
        PointType mousePt = event.getPosition().toFloat();
        const PointType modelSpaceMousePt = transformPointFromScreenSpace(mousePt);
        addPoint(modelSpaceMousePt);
    }

    template <typename T>
    void CurveEditor<T>::mouseMove(const MouseEvent& event)
    {
        repaint();
    }

    template <typename T>
    void CurveEditor<T>::resized()
    {
        screenSpaceTransform = AffineTransform();
        screenSpaceTransform = screenSpaceTransform.translated(-minX, -maxY);
        screenSpaceTransform = screenSpaceTransform.scaled(static_cast<float> (getWidth()) / (maxX - minX),
                                                           static_cast<float> (getHeight()) / (minY - maxY));
    }

    template <typename T>
    void CurveEditor<T>::addPoint(const PointType& p)
    {
        for (size_t i = 0; i < points.size(); i++)
        {
            const auto& point = points[i];
            if (p.x <= point.x)
            {
                points.emplace(points.begin() + i, p);
                repaint();
                return;
            }
        }
    }

    template <typename T>
    float CurveEditor<T>::compute(T input)
    {
        jassert(points.size() > 1);
        for (size_t i = 1; i < points.size(); i++)
        {
            const auto& lastPoint = points[i - 1];
            const auto& point = points[i];

            jassert(lastPoint.x <= point.x);

            if (input <= point.x)
            {
                const auto slope = (point.y - lastPoint.y) / (point.x - lastPoint.x);
                return slope * (input - lastPoint.x) + lastPoint.y;
            }
        }
        jassertfalse; // TODO
        return 0.0f;
    }

    template <typename T>
    typename CurveEditor<T>::PointType CurveEditor<T>::transformPointToScreenSpace(const PointType& p) const
    {
        return p.transformedBy(screenSpaceTransform);
    }

    template <typename T>
    typename CurveEditor<T>::PointType CurveEditor<T>::transformPointFromScreenSpace(const PointType& p) const
    {
        return p.transformedBy(screenSpaceTransform.inverted());
    }
}

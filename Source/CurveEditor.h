#pragma once
#include <JuceHeader.h>

namespace aas
{
    template <typename T>
    struct CurveEditorModel
    {
        using PointType = juce::Point<T>;

        explicit CurveEditorModel(T minX, T maxX, T minY, T maxY) :
            minX (minX),
            maxX (maxX),
            minY (minY),
            maxY (maxY),
            lastInputValue (T (0))
        {
            points.emplace_back (PointType{minX, minY});
            points.emplace_back (PointType{minX + (maxX - minX) * (T)0.5, minY + (maxY - minY) * (T)0.5});
            points.emplace_back (PointType{maxX, maxY});
        }

        /**
         * Map an input value onto the curve
         */
        T compute(T input);

        T minX, maxX;
        T minY, maxY;
        std::vector<PointType> points;
        juce::Value lastInputValue;
    };

    template <typename T>
    T CurveEditorModel<T>::compute(T input)
    {
        jassert (points.size() > 1);
        for (size_t i = 1; i < points.size(); i++)
        {
            const auto& lastPoint = points[i - 1];
            const auto& point = points[i];

            jassert (lastPoint.x <= point.x);

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
    class CurveEditor : public juce::Component, juce::Value::Listener
    {
        using PointType = typename CurveEditorModel<T>::PointType;
    public:
        explicit CurveEditor(CurveEditorModel<T>& model) :
            model (model)
        {
            lastInputValue.referTo (model.lastInputValue);
            lastInputValue.addListener (this);
        }

        void paint(Graphics& g) override;
        void mouseDown(const MouseEvent& event) override;
        void mouseDrag(const MouseEvent& event) override;
        void mouseUp(const MouseEvent& event) override;
        void mouseDoubleClick(const MouseEvent& event) override;
        void mouseMove(const MouseEvent& event) override;
        void resized() override;
        void valueChanged(Value& value) override;

        void addPoint(const PointType& p);

    private:
        PointType transformPointToScreenSpace(const PointType& p) const;
        PointType transformPointFromScreenSpace(const PointType& p) const;
    private:
        const float pointSize = 10.0f;
        juce::AffineTransform screenSpaceTransform;
        PointType* selectedPoint = nullptr;
        CurveEditorModel<T>& model;
        Value lastInputValue;
    };

    template <typename T>
    void CurveEditor<T>::paint(Graphics& g)
    {
        g.setColour (Colours::black);
        g.fillRect (0, 0, getWidth(), getHeight());

        // Draw the actual curve
        Path curve;
        for (size_t i = 0; i < model.points.size(); i++)
        {
            const auto transformedPoint = transformPointToScreenSpace (model.points[i]);
            if (i == 0)
                curve.startNewSubPath (transformedPoint);
            else
                curve.lineTo (transformedPoint);
            if (selectedPoint == &model.points[i])
            {
                g.setColour (Colours::red);
                g.fillEllipse (transformedPoint.x - pointSize * 0.5f,
                               transformedPoint.y - pointSize * 0.5f,
                               pointSize, pointSize);
            }
            else
            {
                g.setColour (Colours::goldenrod);
                g.drawEllipse (transformedPoint.x - pointSize * 0.5f,
                               transformedPoint.y - pointSize * 0.5f,
                               pointSize, pointSize, 3.0f);
            }
        }

        g.setColour (Colours::whitesmoke);
        g.strokePath (curve, PathStrokeType (1.0f));

        // Draw reference line from the mouse pointer to the curve
        const PointType screenSpaceMousePt = getMouseXYRelative().toFloat();
        g.setColour (Colours::red);
        if (contains (getMouseXYRelative()))
        {
            const auto modelSpaceMousePt = transformPointFromScreenSpace (screenSpaceMousePt);
            const auto modelSpaceCurvePt = PointType (modelSpaceMousePt.x, model.compute (modelSpaceMousePt.x));
            const auto screenSpaceCurvePt = transformPointToScreenSpace (modelSpaceCurvePt);

            g.drawVerticalLine (static_cast<int> (screenSpaceMousePt.x),
                                jmin (screenSpaceCurvePt.y, screenSpaceMousePt.y),
                                jmax (screenSpaceCurvePt.y, screenSpaceMousePt.y));

            std::ostringstream ostr;
            ostr << std::fixed << std::setprecision (0) << "[" << modelSpaceCurvePt.x << ", " << modelSpaceCurvePt.y << "]";
            g.drawSingleLineText (ostr.str(), (int)screenSpaceMousePt.x, (int)screenSpaceMousePt.y);
        }

        // Draw reference line for most recent input/output
        g.setColour (Colours::lightblue);
        T inputValue = lastInputValue.getValue();
        T outputValue = model.compute (inputValue);
        const auto screenSpaceCurvePt = transformPointToScreenSpace (PointType (inputValue, outputValue));
        g.drawVerticalLine (static_cast<int> (screenSpaceCurvePt.x), screenSpaceCurvePt.y, static_cast<float> (getHeight()));
        std::ostringstream ostr;
        ostr << std::fixed << std::setprecision (0) << "[" << inputValue << ", " << outputValue << "]";
        g.drawSingleLineText (ostr.str(), (int)screenSpaceCurvePt.x, (int)screenSpaceCurvePt.y);


        // Draw grid
        auto numXTicks = 10; // TODO: Make these editable parameters
        auto numYTicks = 10;
        const Colour slightWhite = Colour::fromRGBA (200, 200, 200, 100);
        g.setColour (slightWhite);
        for (auto i = 0; i < numXTicks; i++)
        {
            T currX = (model.maxX - model.minX) / static_cast<T> (numXTicks) * static_cast<T> (i) + model.minX;
            PointType screenX = transformPointToScreenSpace (PointType (currX, 0));
            g.drawVerticalLine (static_cast<int> (screenX.x), 0.0f, static_cast<float> (getHeight()));
        }
        for (auto i = 0; i < numYTicks; i++)
        {
            T currY = (model.maxY - model.minY) / static_cast<T> (numYTicks) * static_cast<T> (i) + model.minY;
            PointType screenY = transformPointToScreenSpace (PointType (0, currY));
            g.drawHorizontalLine (static_cast<int> (screenY.y), 0.0f, static_cast<float> (getWidth()));
        }
    }

    template <typename T>
    void CurveEditor<T>::mouseDown(const MouseEvent& event)
    {
        jassert (!model.points.empty());

        const auto mousePt = event.getPosition().toFloat();
        PointType* closestPoint = nullptr;
        float closestPointDist = 0;
        const float distanceThreshold = pointSize * 2.0f;
        for (auto& p : model.points)
        {
            const auto dist = transformPointToScreenSpace (p).getDistanceFrom (mousePt);
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
            const PointType mousePt = event.getPosition().toFloat();
            const PointType modelSpaceMousePt = transformPointFromScreenSpace (mousePt);

            // Adjust selected point within the X and Y boundaries
            *selectedPoint = *selectedPoint + 0.9f * (modelSpaceMousePt - *selectedPoint);
            selectedPoint->setX (jlimit (model.minX, model.maxX, selectedPoint->getX()));
            selectedPoint->setY (jlimit (model.minY, model.maxY, selectedPoint->getY()));

            // Lock the X position of the first and last points
            if (selectedPoint == &model.points.front())
            {
                selectedPoint->setX (model.minX);
            }
            else if (selectedPoint == &model.points.back())
            {
                selectedPoint->setX (model.maxX);
            }

            // Allow points to be seamlessly dragged passed each other
            if (selectedPoint > &model.points.front() && selectedPoint->x < (selectedPoint - 1)->x)
            {
                std::swap (*selectedPoint, *(selectedPoint - 1));
                selectedPoint = selectedPoint - 1;
            }
            else if (selectedPoint < &model.points.back() && selectedPoint->x > (selectedPoint + 1)->x)
            {
                std::swap (*selectedPoint, *(selectedPoint + 1));
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
        const PointType modelSpaceMousePt = transformPointFromScreenSpace (mousePt);
        addPoint (modelSpaceMousePt);
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
        screenSpaceTransform = screenSpaceTransform.translated (-model.minX, -model.maxY);
        screenSpaceTransform = screenSpaceTransform.scaled (static_cast<float> (getWidth()) / (model.maxX - model.minX),
                                                            static_cast<float> (getHeight()) / (model.minY - model.maxY
                                                            ));
    }

    template <typename T>
    void CurveEditor<T>::valueChanged(Value& value)
    {
        repaint();
    }

    template <typename T>
    void CurveEditor<T>::addPoint(const PointType& p)
    {
        for (size_t i = 0; i < model.points.size(); i++)
        {
            const auto& point = model.points[i];
            if (p.x <= point.x)
            {
                model.points.emplace (model.points.begin() + i, p);
                repaint();
                return;
            }
        }
    }

    template <typename T>
    typename CurveEditor<T>::PointType CurveEditor<T>::transformPointToScreenSpace(const PointType& p) const
    {
        return p.transformedBy (screenSpaceTransform);
    }

    template <typename T>
    typename CurveEditor<T>::PointType CurveEditor<T>::transformPointFromScreenSpace(const PointType& p) const
    {
        return p.transformedBy (screenSpaceTransform.inverted());
    }
}

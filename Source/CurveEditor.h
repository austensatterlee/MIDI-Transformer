#pragma once
#include <JuceHeader.h>

namespace aas
{
    template <typename T>
    struct CurveEditorModel {
        using NumericType = T;
        using PointType = juce::Point<T>;

        enum class CurveType {
            Linear = 0,
            Quadratic
        };

        static constexpr int CurveTypeCount = 2;

        struct Handle;

        struct Node {
            Handle anchor;
            Handle control1;
            CurveType curveType = CurveType::Linear;

            explicit Node(const PointType& anchor) :
                anchor (Handle{anchor, this}),
                control1 (Handle{anchor, this}) { }

            void setAnchorPt(const PointType& pt) {
                auto anchorControlDist = anchor.pt - control1.pt;
                anchor.pt = pt;
                control1.pt = pt - anchorControlDist;
            }

            void setControlPt(const PointType& pt) {
                control1.pt = pt;
            }
        };

        struct Handle {
            PointType pt;
            Node* parent;

            Handle(const PointType& pt, Node* parent) :
                pt (pt),
                parent (parent) { }

            void setX(T newX) { pt.setX (newX); }
            void setY(T newY) { pt.setY (newY); }
        };

        explicit CurveEditorModel(T minX, T maxX, T minY, T maxY) :
            minX (minX),
            maxX (maxX),
            minY (minY),
            maxY (maxY),
            lastInputValue (static_cast<T> (0)) {
            nodes.emplace_back (std::make_shared<Node> (PointType{minX, minY}));
            nodes.emplace_back (std::make_shared<Node> (PointType{
                                                            minX + (maxX - minX) * static_cast<T> (0.5),
                                                            minY + (maxY - minY) * static_cast<T> (0.5)
                                                        }));
            nodes.emplace_back (std::make_shared<Node> (PointType{maxX, maxY}));
        }

        /**
         * Map an input value onto the curve
         */
        T compute(T input);

        T minX, maxX;
        T minY, maxY;
        std::vector<std::shared_ptr<Node>> nodes;
        juce::Value lastInputValue;
    };

    template <typename T>
    T CurveEditorModel<T>::compute(T input) {
        jassert (nodes.size() > 1);
        for (size_t i = 1; i < nodes.size(); i++) {
            const auto& lastNode = *nodes[i - 1];
            const auto& node = *nodes[i];

            const auto& lastAnchorPoint = lastNode.anchor.pt;
            const auto& anchorPoint = node.anchor.pt;

            jassert (lastAnchorPoint.x <= anchorPoint.x);

            if (input <= anchorPoint.x) {
                switch (lastNode.curveType) {
                case CurveType::Quadratic:
                    // B(t) = (1-t)^2 * P0 + 2(1-t) * t * P1 + t^2 * P2 , 0 < t < 1
                    //       P0-P1 [+/-] sqrt(P1^2 - P0 * P2)
                    // t =  -----------------------
                    //         P0(P0 - 2P1 + P2)
                    {
                        const auto& ctrlPoint = lastNode.control1.pt;
                        float finalT = 0.0f;
                        float minDist = -1.0f;
                        for (int j = 0; j <= 100; j++) {
                            const float t = static_cast<float> (j) / 100.0f;
                            const float x = std::pow ((1 - t), 2.0) * lastAnchorPoint.x + 2 * (1 - t) * t * ctrlPoint.x + std::pow (t, 2.0) * anchorPoint.x;
                            const float distance = std::abs (x - input);
                            if (distance < minDist || minDist < 0.0f) {
                                finalT = t;
                                minDist = distance;
                            }
                        }
                        float y = std::pow ((1 - finalT), 2.0) * lastAnchorPoint.y + 2 * (1 - finalT) * finalT * ctrlPoint.y +
                                std::pow (finalT, 2.0) * anchorPoint.y;
                        return y;
                    }
                default:
                case CurveType::Linear:
                    const auto slope = (anchorPoint.y - lastAnchorPoint.y) / (anchorPoint.x - lastAnchorPoint.x);
                    return slope * (input - lastAnchorPoint.x) + lastAnchorPoint.y;
                }
            }
        }
        jassertfalse; // TODO
        return 0.0f;
    }

    template <typename T>
    class CurveEditor : public juce::Component, juce::Value::Listener {
        using PointType = typename CurveEditorModel<T>::PointType;
        using Handle = typename CurveEditorModel<T>::Handle;
        using Node = typename CurveEditorModel<T>::Node;
        using CurveType = typename CurveEditorModel<T>::CurveType;
    public:
        explicit CurveEditor(CurveEditorModel<T>& model) :
            model (model) {
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
        /**
         * \brief Get a reference to the handle closest to the given point (in screen space)
         */
        Handle* getClosestHandle(const PointType& screenPt);

    private:
        PointType transformPointToScreenSpace(const PointType& p) const;
        PointType transformPointFromScreenSpace(const PointType& p) const;
    private:
        const float POINT_SIZE = 10.0f;
        const float DISTANCE_THRESHOLD = POINT_SIZE * 2.0f;
        juce::AffineTransform screenSpaceTransform;
        Handle* selectedHandle = nullptr;
        CurveEditorModel<T>& model;
        Value lastInputValue;
    };

    template <typename T>
    void CurveEditor<T>::paint(Graphics& g) {
        g.setColour (Colours::black);
        g.fillRect (0, 0, getWidth(), getHeight());

        auto drawHandles = [this, &g](const Node& node)
        {
            auto transformedAnchorPoint = transformPointToScreenSpace (node.anchor.pt);
            if (selectedHandle == &node.anchor) {
                g.setColour (Colours::red);
                g.fillEllipse (transformedAnchorPoint.x - POINT_SIZE * 0.5f,
                               transformedAnchorPoint.y - POINT_SIZE * 0.5f,
                               POINT_SIZE, POINT_SIZE);
            }
            else {
                g.setColour (Colours::goldenrod);
                g.drawEllipse (transformedAnchorPoint.x - POINT_SIZE * 0.5f,
                               transformedAnchorPoint.y - POINT_SIZE * 0.5f,
                               POINT_SIZE, POINT_SIZE, 3.0f);
            }

            if (node.curveType == CurveType::Quadratic) {
                auto transformedControlPoint = transformPointToScreenSpace (node.control1.pt);
                if (selectedHandle == &node.control1) {
                    g.setColour (Colours::red);
                    g.fillEllipse (transformedControlPoint.x - POINT_SIZE * 0.5f,
                                   transformedControlPoint.y - POINT_SIZE * 0.5f,
                                   POINT_SIZE, POINT_SIZE);
                }
                else {
                    g.setColour (Colours::goldenrod);
                    g.drawEllipse (transformedControlPoint.x - POINT_SIZE * 0.5f,
                                   transformedControlPoint.y - POINT_SIZE * 0.5f,
                                   POINT_SIZE, POINT_SIZE, 3.0f);
                }
                g.drawLine (transformedAnchorPoint.x, transformedAnchorPoint.y, transformedControlPoint.x, transformedControlPoint.y);
            }
        };

        // Draw the actual curve
        Path curve;
        for (size_t i = 0; i < model.nodes.size(); i++) {
            const auto transformedAnchorPoint = transformPointToScreenSpace (model.nodes[i]->anchor.pt);
            if (i == 0)
                curve.startNewSubPath (transformedAnchorPoint);
            else {
                if (model.nodes[i - 1]->curveType == CurveType::Linear) {
                    curve.lineTo (transformedAnchorPoint);
                }
                else if (model.nodes[i - 1]->curveType == CurveType::Quadratic) {
                    const auto transformedControlPoint = transformPointToScreenSpace (model.nodes[i - 1]->control1.pt);
                    curve.quadraticTo (transformedControlPoint.x, transformedControlPoint.y, transformedAnchorPoint.x,
                                       transformedAnchorPoint.y);
                }
            }
            drawHandles (*model.nodes[i]);
        }

        g.setColour (Colours::whitesmoke);
        g.strokePath (curve, PathStrokeType (1.0f));

        // Draw reference line from the mouse pointer to the curve
        const PointType screenSpaceMousePt = getMouseXYRelative().toFloat();
        g.setColour (Colours::red);
        if (contains (getMouseXYRelative())) {
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
        for (auto i = 0; i < numXTicks; i++) {
            T currX = (model.maxX - model.minX) / static_cast<T> (numXTicks) * static_cast<T> (i) + model.minX;
            PointType screenX = transformPointToScreenSpace (PointType (currX, 0));
            g.drawVerticalLine (static_cast<int> (screenX.x), 0.0f, static_cast<float> (getHeight()));
        }
        for (auto i = 0; i < numYTicks; i++) {
            T currY = (model.maxY - model.minY) / static_cast<T> (numYTicks) * static_cast<T> (i) + model.minY;
            PointType screenY = transformPointToScreenSpace (PointType (0, currY));
            g.drawHorizontalLine (static_cast<int> (screenY.y), 0.0f, static_cast<float> (getWidth()));
        }
    }

    template <typename T>
    void CurveEditor<T>::mouseDown(const MouseEvent& event) {
        jassert (!model.nodes.empty());

        Handle* closestHandle = getClosestHandle (event.mouseDownPosition);
        if (!closestHandle)
            return;

        const PointType& closestPt = closestHandle->pt;

        auto closestPointDist = transformPointToScreenSpace (closestPt).getDistanceFrom (event.mouseDownPosition);


        if (closestPointDist < DISTANCE_THRESHOLD) {
            if (event.mods.isLeftButtonDown()) {
                selectedHandle = closestHandle;
            }
            else if (event.mods.isRightButtonDown()) {
                if (closestHandle->parent != model.nodes.front().get() && closestHandle->parent != model.nodes.back().get()) {
                    int toErase = -1;
                    for (int i = 0; i < model.nodes.size(); i++) {
                        if (model.nodes[i].get() == closestHandle->parent) {
                            toErase = i;
                            break;
                        }
                    }
                    if (toErase != -1) {
                        model.nodes.erase (model.nodes.begin() + toErase);
                    }
                    selectedHandle = nullptr;
                }
            }
        }
        else {
            selectedHandle = nullptr;
        }

        repaint();
    }

    template <typename T>
    void CurveEditor<T>::mouseDrag(const MouseEvent& event) {
        if (selectedHandle) {
            const PointType mousePt = event.getPosition().toFloat();
            const PointType modelSpaceMousePt = transformPointFromScreenSpace (mousePt);
            PointType selectedPoint = selectedHandle->pt;
            CurveType curveType = selectedHandle->parent->curveType;

            // Adjust selected point within the X and Y boundaries
            selectedPoint = selectedPoint + 0.9f * (modelSpaceMousePt - selectedPoint);
            selectedPoint.setX (jlimit (model.minX + 1, model.maxX - 1, selectedPoint.getX()));
            selectedPoint.setY (jlimit (model.minY, model.maxY, selectedPoint.getY()));

            // Lock the X position of the first and last points
            if (selectedHandle->parent == model.nodes.front().get()) {
                selectedPoint.setX (model.minX);
            }
            else if (selectedHandle->parent == model.nodes.back().get()) {
                selectedPoint.setX (model.maxX);
            }

            if (selectedHandle == &selectedHandle->parent->anchor) {
                selectedHandle->parent->setAnchorPt (selectedPoint);
            }
            else if (selectedHandle == &selectedHandle->parent->control1 && curveType == CurveType::Quadratic) {
                selectedHandle->parent->setControlPt (selectedPoint);
            }

            repaint();
        }
    }

    template <typename T>
    void CurveEditor<T>::mouseUp(const MouseEvent& event) {
        selectedHandle = nullptr;
    }

    template <typename T>
    void CurveEditor<T>::mouseDoubleClick(const MouseEvent& event) {
        const PointType mousePt = event.mouseDownPosition;
        Handle* closestHandle = getClosestHandle (mousePt);
        if (!closestHandle)
            return;

        const PointType& closestPt = closestHandle->pt;
        Node* closestNode = closestHandle->parent;

        auto closestPointDist = transformPointToScreenSpace (closestPt).getDistanceFrom (mousePt);

        if (closestPointDist < DISTANCE_THRESHOLD && closestHandle == &closestNode->anchor) {
            CurveType newType = static_cast<CurveType> ((static_cast<int> (closestNode->curveType) + 1) % CurveEditorModel<
                T>::CurveTypeCount);
            if (newType == CurveType::Linear) {
                closestNode->curveType = newType;
                closestNode->setControlPt (closestNode->anchor.pt);
            }
            if (newType == CurveType::Quadratic && closestNode != model.nodes.back().get()) {
                closestNode->curveType = newType;
                PointType controlPoint = closestNode->anchor.pt + PointType (5, 0);
                closestHandle->parent->setControlPt (controlPoint);
            }
        }
        else {
            const PointType modelSpaceMousePt = transformPointFromScreenSpace (mousePt);
            addPoint (modelSpaceMousePt);
        }
    }

    template <typename T>
    void CurveEditor<T>::mouseMove(const MouseEvent& event) {
        repaint();
    }

    template <typename T>
    void CurveEditor<T>::resized() {
        screenSpaceTransform = AffineTransform();
        screenSpaceTransform = screenSpaceTransform.translated (-model.minX, -model.maxY);
        screenSpaceTransform = screenSpaceTransform.scaled (static_cast<float> (getWidth()) / (model.maxX - model.minX),
                                                            static_cast<float> (getHeight()) / (model.minY - model.maxY
                                                            ));
    }

    template <typename T>
    void CurveEditor<T>::valueChanged(Value& value) {
        repaint();
    }

    template <typename T>
    void CurveEditor<T>::addPoint(const PointType& p) {
        for (size_t i = 0; i < model.nodes.size(); i++) {
            const auto& point = *model.nodes[i];
            if (p.x <= point.anchor.pt.x) {
                model.nodes.emplace (model.nodes.begin() + i, std::make_shared<Node> (p));
                repaint();
                return;
            }
        }
    }

    template <typename T>
    typename CurveEditor<T>::Handle* CurveEditor<T>::getClosestHandle(const PointType& screenPt) {
        jassert (!model.nodes.empty());

        auto modelPt = transformPointFromScreenSpace (screenPt);
        Handle* closestHandle = nullptr;
        float closestHandleDist = 0;
        for (auto& node : model.nodes) {
            auto dist = modelPt.getDistanceFrom (node->anchor.pt);
            if (dist < closestHandleDist || closestHandle == nullptr) {
                closestHandle = &node->anchor;
                closestHandleDist = dist;
            }

            if (node->curveType == CurveType::Quadratic) {
                dist = modelPt.getDistanceFrom (node->control1.pt);
                if (dist < closestHandleDist || closestHandle == nullptr) {
                    closestHandle = &node->control1;
                    closestHandleDist = dist;
                }
            }
        }

        return closestHandle;
    }

    template <typename T>
    typename CurveEditor<T>::PointType CurveEditor<T>::transformPointToScreenSpace(const PointType& p) const {
        return p.transformedBy (screenSpaceTransform);
    }

    template <typename T>
    typename CurveEditor<T>::PointType CurveEditor<T>::transformPointFromScreenSpace(const PointType& p) const {
        return p.transformedBy (screenSpaceTransform.inverted());
    }
}

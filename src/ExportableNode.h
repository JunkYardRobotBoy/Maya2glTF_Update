#pragma once

#include "ExportableCamera.h"
#include "ExportableMesh.h"
#include "ExportableObject.h"
#include "ExportableScene.h"
#include "Transform.h"

enum class TransformKind {
    // A simple transform without segment scale compensation nor pivot point
    Simple,

    // A joint with segment scale compensation
    ComplexJoint,

    // A transform with a pivot point, represented by two glTF nodes:
    // outer = (translation + pivot, rotation, scale), inner ":PIV" = -pivot
    ComplexTransform,

    // A transform with a pivot point, represented by a single glTF node whose
    // origin is the Maya pivot: node = (translation + pivot, rotation, scale),
    // while the -pivot offset is baked into the mesh vertices (and into the
    // local transforms of child nodes). See ExportableNode::meshBakeOffset.
    BakedPivot
};

class ExportableNode : public ExportableObject {
  public:
    ~ExportableNode();

    // Mesh attached to node, or null
    ExportableMesh *mesh() const { return m_mesh.get(); }

    // Camera attached to node, or null
    ExportableCamera *camera() const { return m_camera.get(); }

    bool hasAttachedShape() const { return m_mesh || m_camera; }

    const MDagPath dagPath;

    TransformKind transformKind = TransformKind::Simple;

    double scaleFactor = 1.0;
    double posPrecision = 1e9;
    double dirPrecision = 1e9;
    double sclPrecision = 1e9;

    MPoint pivotPoint;

    // True when the Maya transform needs two glTF nodes.
    bool hasExtraNode() const {
        return transformKind == TransformKind::ComplexJoint || transformKind == TransformKind::ComplexTransform;
    }

    // Object-space offset to subtract from the vertex positions of meshes
    // attached to this node, so that the glTF node origin is the Maya pivot.
    // Zero unless this node (or, for a shape node, its parent transform) is a
    // BakedPivot transform.
    MVector meshBakeOffset() const;

    // nullptr for root nodes.
    ExportableNode *parentNode = nullptr;

    std::vector<ExportableNode *> children;

    NodeTransformState initialTransformState;
    NodeTransformState currentTransformState;

    std::unique_ptr<NodeAnimation> createAnimation(const Arguments &args, const ExportableFrames &frameTimes,
                                                   double scaleFactor) override;

    // The primary node to represent the transform
    // See Transform.h for details
    GLTF::Node &glPrimaryNode() { return m_glNodes[0]; }
    const GLTF::Node &glPrimaryNode() const { return const_cast<ExportableNode *>(this)->glPrimaryNode(); }

    // The secondary node to represent the transform
    // Can be the same as the primary node for simple transforms
    // See Transform.h for details
    GLTF::Node &glSecondaryNode() { return m_glNodes[hasExtraNode()]; }
    const GLTF::Node &glSecondaryNode() const { return const_cast<ExportableNode *>(this)->glSecondaryNode(); }

    MDagPath parentDagPath() const { return parentNode ? parentNode->dagPath : MDagPath(); }

    // Update the node transforms using the values at the current frame
    void updateNodeTransforms(NodeTransformCache &transformCache);

    // If this node is a redundant shape node, move the mesh to the parent node,
    // and return true.
    bool tryMergeRedundantShapeNode();

    // Returns true if this node is animated.
    bool isAnimated() const;

    void getAllAccessors(std::vector<GLTF::Accessor *> &accessors) const;

  private:
    friend class ExportableScene;

    ExportableNode(const MDagPath &dagPath);

    void load(ExportableScene &scene, NodeTransformCache &transformCache);

    std::array<GLTF::Node, 2> m_glNodes;
    std::unique_ptr<ExportableMesh> m_mesh;
    std::unique_ptr<ExportableCamera> m_camera;

    DISALLOW_COPY_MOVE_ASSIGN(ExportableNode);
};

"""
maya2glTF_mash
==============

Converts a MASH network into real DAG instances of its source meshes.

The maya2glTF exporter writes DAG-instanced shapes as ONE glTF mesh referenced
by many nodes, so a scene converted this way exports to a small .glb that a
WebGPU / WebGL viewer can draw with hardware instancing (few draw calls).

Usage (from the Maya2glTF UI button, or the script editor):

    import maya2glTF_mash
    maya2glTF_mash.createMashInstances()

Select any node of the MASH network (the MASH_Waiter, the Repro/Instancer
output, a MASH_Distribute node, ...) and run. Instances are parented under a
group called "<waiterName>_Instances". Running again on the same network
rebuilds that group instead of adding a second set of instances.

Point data is read exactly the way Autodesk's own MASH.api.Network reads it:
the Waiter's "outputPoints" plug holds an MFnArrayAttrsData with the channels
"position", "rotation" (degrees), "scale", "objectIndex" and "visibility".
"""

import math
import maya.cmds as mc
import maya.OpenMaya as OpenMaya

MASH_NODE_TYPES = ('MASH_Waiter', 'MASH_Repro', 'instancer')


def error(message):
    OpenMaya.MGlobal.displayError("createMashInstances: %s" % message)


def info(message):
    OpenMaya.MGlobal.displayInfo("createMashInstances: %s" % message)


def getMObject(name):
    slist = OpenMaya.MSelectionList()
    try:
        slist.add(name)
    except RuntimeError:
        return None
    mobj = OpenMaya.MObject()
    slist.getDependNode(0, mobj)
    return mobj


def shortName(name):
    return name.split("|")[-1].split(":")[-1]


# ---------------------------------------------------------------------------
# Network discovery
# ---------------------------------------------------------------------------

def findWaiter(selection):
    """Trace a MASH_Waiter from any node of the network."""
    for sel in selection:
        if mc.nodeType(sel) == 'MASH_Waiter':
            return sel

        # A transform holding a waiter shape (or the Repro / instancer shape)
        shapes = mc.listRelatives(sel, shapes=True, fullPath=True) or []
        for shape in shapes:
            if mc.nodeType(shape) == 'MASH_Waiter':
                return shape

        # Repro / instancer -> waiter via the message connection MASH itself uses
        for node in [sel] + shapes:
            if mc.nodeType(node) in ('MASH_Repro', 'instancer'):
                conns = mc.listConnections(node + '.instancerMessage', s=True, d=False) or [] \
                    if mc.attributeQuery('instancerMessage', node=node, exists=True) else []
                conns += mc.listConnections(node, s=True, d=False, type='MASH_Waiter') or []
                if conns:
                    return conns[0]

        # Any other MASH node: walk the history / future for a waiter
        for node in (mc.listHistory(sel) or []) + (mc.listHistory(sel, future=True) or []):
            if mc.nodeType(node) == 'MASH_Waiter':
                return node

    return None


def findOutputNode(waiter):
    """Return the MASH_Repro or instancer node driven by this waiter, or None."""
    conns = mc.listConnections(waiter + '.instancerMessage', s=False, d=True) or []
    for node in conns:
        if mc.nodeType(node) in ('MASH_Repro', 'instancer'):
            return node
    for node in mc.listConnections(waiter, s=False, d=True) or []:
        if mc.nodeType(node) in ('MASH_Repro', 'instancer'):
            return node
    return None


def findSourceMeshes(outputNode):
    """
    Return the source mesh transforms (long names), in the order MASH indexes
    them with the "objectIndex" channel.
    """
    plugsInOrder = []

    if outputNode and mc.nodeType(outputNode) == 'MASH_Repro':
        # Repro: instancedGroup[i].instancedMesh <- sourceShape.outMesh
        indices = mc.getAttr(outputNode + '.instancedGroup', multiIndices=True) or []
        for i in indices:
            plug = "%s.instancedGroup[%d]" % (outputNode, i)
            conns = mc.listConnections(plug, s=True, d=False, shapes=True) or []
            if not conns and mc.attributeQuery('instancedMesh', node=outputNode, exists=True):
                conns = mc.listConnections(plug + '.instancedMesh', s=True, d=False, shapes=True) or []
            plugsInOrder.append(conns[0] if conns else None)

    elif outputNode and mc.nodeType(outputNode) == 'instancer':
        # Instancer: inputHierarchy[i] <- sourceTransform.matrix
        indices = mc.getAttr(outputNode + '.inputHierarchy', multiIndices=True) or []
        for i in indices:
            plug = "%s.inputHierarchy[%d]" % (outputNode, i)
            conns = mc.listConnections(plug, s=True, d=False) or []
            plugsInOrder.append(conns[0] if conns else None)

    if not any(plugsInOrder) and outputNode:
        # Last resort: anything mesh-like feeding the output node
        conns = (mc.listConnections(outputNode, s=True, d=False, shapes=True, type='mesh') or []) + \
                (mc.listConnections(outputNode, s=True, d=False, type='transform') or [])
        plugsInOrder = list(dict.fromkeys(conns))

    transforms = []
    for item in plugsInOrder:
        if item is None:
            transforms.append(None)
            continue
        if mc.nodeType(item) in MASH_NODE_TYPES:
            transforms.append(None)
            continue
        if mc.nodeType(item) == 'transform':
            longName = mc.ls(item, long=True)[0]
        else:
            parents = mc.listRelatives(item, parent=True, fullPath=True) or []
            longName = parents[0] if parents else None
        transforms.append(longName)

    return transforms


# ---------------------------------------------------------------------------
# Point data
# ---------------------------------------------------------------------------

def extractMashData(waiter):
    """
    Read the waiter's outputPoints array data.
    Returns a dict with lists: position, rotation, scale, objectIndex, visibility.
    Missing channels get sensible defaults. Returns None on failure.
    """
    mobj = getMObject(waiter)
    if mobj is None:
        return None

    depNode = OpenMaya.MFnDependencyNode(mobj)
    if not depNode.hasAttribute("outputPoints"):
        error("Node %s has no outputPoints attribute (not a MASH_Waiter?)." % waiter)
        return None

    plug = depNode.findPlug("outputPoints", False)
    dataObj = plug.asMObject()
    if dataObj.isNull():
        error("outputPoints on %s is empty. Is the MASH network enabled?" % waiter)
        return None

    data = OpenMaya.MFnArrayAttrsData(dataObj)
    channels = list(data.list())

    def vectors(name, default):
        if name not in channels:
            return None
        arr = data.getVectorData(name)
        return [(arr[i].x, arr[i].y, arr[i].z) for i in range(arr.length())]

    def doubles(name):
        if name not in channels:
            return None
        arr = data.getDoubleData(name)
        return [arr[i] for i in range(arr.length())]

    position = vectors("position", (0.0, 0.0, 0.0))
    if position is None:
        error("outputPoints has no 'position' channel. Channels found: %s" % ", ".join(channels))
        return None

    count = len(position)
    rotation = vectors("rotation", (0.0, 0.0, 0.0)) or [(0.0, 0.0, 0.0)] * count
    scale = vectors("scale", (1.0, 1.0, 1.0)) or [(1.0, 1.0, 1.0)] * count
    objectIndex = doubles("objectIndex") or [0.0] * count
    visibility = doubles("visibility") or [1.0] * count

    return {
        "position": position,
        "rotation": rotation,
        "scale": scale,
        "objectIndex": objectIndex,
        "visibility": visibility,
        "channels": channels,
    }


# ---------------------------------------------------------------------------
# Instance creation
# ---------------------------------------------------------------------------

def getOrCreateInstanceGroup(waiter):
    """Group that receives the instances. Existing contents are removed."""
    groupName = "%s_Instances" % shortName(waiter)
    existing = mc.ls(groupName, type='transform', long=True)
    if existing:
        group = existing[0]
        children = mc.listRelatives(group, children=True, fullPath=True) or []
        if children:
            mc.delete(children)
        return group
    return mc.ls(mc.group(empty=True, name=groupName), long=True)[0]


def createMashInstances():
    selected = mc.ls(selection=True, long=True)
    if not selected:
        error("Selection is empty. Select any node of the MASH network.")
        return -1

    waiter = findWaiter(selected)
    if not waiter:
        error("Could not find a MASH_Waiter from the selection.")
        return -1
    waiter = mc.ls(waiter, long=True)[0]
    info("MASH network: %s" % shortName(waiter))

    outputNode = findOutputNode(waiter)
    if not outputNode:
        error("No MASH_Repro or instancer node is connected to %s." % shortName(waiter))
        return -1

    sourceMeshes = findSourceMeshes(outputNode)
    validSources = [m for m in sourceMeshes if m]
    if not validSources:
        error("No source meshes found on %s." % outputNode)
        return -1

    mashData = extractMashData(waiter)
    if not mashData:
        return -1

    numPoints = len(mashData["position"])
    if numPoints == 0:
        error("MASH network has 0 points.")
        return -1

    fallbackMesh = validSources[0]

    mc.undoInfo(openChunk=True, chunkName="maya2glTF_createMashInstances")
    try:
        group = getOrCreateInstanceGroup(waiter)

        created = 0
        skippedHidden = 0
        for idx in range(numPoints):
            if mashData["visibility"][idx] < 0.5:
                skippedHidden += 1
                continue

            objIdx = int(math.floor(mashData["objectIndex"][idx] + 0.5))
            source = None
            if 0 <= objIdx < len(sourceMeshes):
                source = sourceMeshes[objIdx]
            if source is None:
                source = fallbackMesh

            inst = mc.instance(source, name="%s_Instance_%d" % (shortName(source), idx))[0]
            inst = mc.parent(inst, group)[0]
            inst = mc.ls(inst, long=True)[0]

            px, py, pz = mashData["position"][idx]
            rx, ry, rz = mashData["rotation"][idx]
            sx, sy, sz = mashData["scale"][idx]
            mc.setAttr(inst + ".translate", px, py, pz, type="double3")
            mc.setAttr(inst + ".rotate", rx, ry, rz, type="double3")
            mc.setAttr(inst + ".scale", sx, sy, sz, type="double3")
            mc.setAttr(inst + ".visibility", True)
            created += 1
    finally:
        mc.undoInfo(closeChunk=True)

    msg = "Created %d instances under %s from %d source mesh(es)." % (created, shortName(group), len(validSources))
    if skippedHidden:
        msg += " Skipped %d hidden points." % skippedHidden
    info(msg)
    info("Tip: hide the MASH network's %s node before exporting so it is not exported twice." % outputNode)
    return 0

import math
import maya.cmds as mc
import maya.OpenMaya as OpenMaya

def error(message):
    fullMsg = "createMashInstances: %s" % message
    OpenMaya.MGlobal.displayError(fullMsg)

def info(message):
    fullMsg = "createMashInstances: %s" % message
    OpenMaya.MGlobal.displayInfo(fullMsg)

def getMObject(name):
    slist = OpenMaya.MSelectionList()
    try:
        slist.add(name)
    except:
        return None

    mobj = OpenMaya.MObject()
    slist.getDependNode(0, mobj)
    
    return mobj

def getMFnDependencyNode(mobject):
    return OpenMaya.MFnDependencyNode(mobject)

def getAttributeData(node, attribute):
    mfnDepNode = getMFnDependencyNode(node)
    if (mfnDepNode == None or (mfnDepNode.hasAttribute(attribute) == False)):
        return None

    attr = mfnDepNode.attribute(attribute)
    plug = OpenMaya.MPlug(node, attr)

    try:
        handleData = plug.asMDataHandle().data()
    except:
        return None

    arrayData = OpenMaya.MFnArrayAttrsData(handleData)
    return arrayData

def getChannelAsList(channel):
    if channel == None or channel.length() == 0:
        return None

    listOfVectors = []
    if channel.__class__.__name__ == "MVectorArray":
        for i in range(channel.length()):
            vec = (channel[i].x, channel[i].y, channel[i].z)
            listOfVectors.append(vec)
    return listOfVectors

def createLocatorIfNotFound(mashNodeName):
    locatorName = "%s_Instances" % mashNodeName
    locatorObj = getMObject(locatorName)
    if locatorObj is None:
        try:
            locFn = OpenMaya.MFnTransform()
            locObj = locFn.create()
            locFn.setName(locatorName)
            return locFn.fullPathName()
        except:
            error(f"Failed to create locator: {locatorName}")
            return None
    return locatorName

def createMashInstances():
    # Get selected MASH node
    selected = mc.ls(selection=True)
    if not selected or 'MASH' not in selected[0]:
        error("Please select a MASH node.")
        return -1
    mashNodeName = selected[0]

    transformNode = createLocatorIfNotFound(mashNodeName)
    if not transformNode:
        error("Failed to create or find locator.")
        return -1
    
    mashNode = getMObject(mashNodeName)
    if not mashNode:
        error(f"Failed to find mash node: {mashNodeName}")
        return -1
    attrData = getAttributeData(mashNode, "inputPoints")

    idxArray = attrData.getDoubleData("objectIndex")
    posArray = attrData.getVectorData("position")
    rotArray = attrData.getVectorData("rotation")
    sclArray = attrData.getVectorData("scale")

    parentXform = getMObject(transformNode)
    if not parentXform:
        error(f"Failed to find parent transform: {transformNode}")
        return -1
    parentFn = OpenMaya.MFnTransform(parentXform)

    reproNode = getMObject(f"{mashNodeName}_Repro")
    if not reproNode:
        error(f"Failed to find repro node: {mashNodeName}_Repro")
        return -1

    attachedMeshes = list(set(mc.listConnections(f"{mashNodeName}_Repro.instancedGroup", destination=False, type="mesh")))
    meshNames = mc.ls(attachedMeshes, long=True)
    
    meshArray = []

    for meshName in meshNames:
        meshObj = getMObject(meshName)
        if meshObj:
            meshArray.append(meshObj)
        else:
            error(f"Mesh not found: {meshName}")

    if not len(meshArray):
        error("No valid meshes provided.")
        return -1

    for idx in range(posArray.length()):
        info(f"Instance: mesh [{idx}] id: [{idxArray[idx]}] len mesh array [{len(meshNames)}]")
        instanceNode = meshArray[math.floor(idxArray[idx])]
        if not instanceNode:
            error("Failed to find instance node.")
            return -1

        instanceNodeFn = OpenMaya.MFnTransform(instanceNode)

        instance = instanceNodeFn.duplicate(True, False)
        instanceFn = OpenMaya.MFnTransform(instance)
        instanceFn.setName("%s_Instance_%s" % (instanceNodeFn.name(), idx))

        parentFn.addChild(instance)
        
        # --- NEW CODE: SET VISIBILITY TO ON ---
        try:
            visPlug = instanceFn.findPlug('visibility', False)
            visPlug.setBool(True)
        except:
            error(f"Failed to set visibility for instance {idx}")
        # --------------------------------------

        # Apply translation, rotation, and scale
        instanceTranslatePlug = instanceFn.findPlug('translate', False)
        for x in range(3):
            instanceTranslatePlug.child(x).setDouble(posArray[idx][x])

        instanceRotationPlug = instanceFn.findPlug('rotate', False)
        for x in range(3):
            instanceRotationPlug.child(x).setMAngle(OpenMaya.MAngle(rotArray[idx][x], OpenMaya.MAngle.kDegrees))

        instanceScalePlug = instanceFn.findPlug('scale', False)
        instanceScalePlug.child(0).setFloat(sclArray[idx][0])
        instanceScalePlug.child(1).setFloat(sclArray[idx][1])
        instanceScalePlug.child(2).setFloat(sclArray[idx][2])

    return 0

# Call the export function
createMashInstances()
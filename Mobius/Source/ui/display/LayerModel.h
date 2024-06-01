/**
 * A conceptual model for visualizing loop layers.
 * Wraps the internal MobiusState models and provides a more
 * convenient interface for the UI.
 *
 * A loop consists of any number of layers ordered by the time they were created.
 * There is always one "active" layer which is what is being heard when the loop plays.
 * The user may move the active layer back and forth in the layer history.
 *
 * Layers created before the active layer are called "undo" layers, layers created
 * after the active layer are "redo" layers.  These are usually visualized
 * in a horizontal row with undo layers to the left of the active layer and redo
 * layers to the right.
 *
 * Layers always have a size representing the number of audio frames they contain.
 * At runtime layers may have additional properties that may be of interest to the user.
 *
 *     active       the layer that is being heard
 *     checkpoint   the layer is marked as a checkpoint
 *     ghost        the layer exists, but detailed information about it is not available
 *
 * Internally, layers are represented by instnaces of the Layer class which is complex.
 * To reduce the amount of data transfer between the engine and the UI, layer information
 * is passed to the UI in a simplified model using the MobiusLoopState and MobiusLayerState
 * classes.  The relevant members are:
 *
 *    MobiusLoopState.layerCount
 *      the number of undo layers plus the active layer for which there is
 *      detailed information included
 *
 *    MobiusLoopState.layerLoss
 *      the number of undo layers that do not have detailed information
 *
 *    MobiusLoopState.redoCount
 *      the number of redo layers that have details
 *
 *    MobiusLoopState.redoLoss
 *      the number of redo layers that do not have details
 *
 * Note that layerCount includes some number of redo layers plus the layer representing
 * the active layer.  So conceptually the states are
 *
 *     undoLayers = layerLoss + layerCount - 1 (for the active layer)
 *     redoLayers = redoCount + redoLoss
 *     totalLayers = undoLayers + redoLayers + 1 (for the active layer)
 *
 * The UI does not need to care about the difference between undo and redo layers,
 * only that some number of ordered layers exist, and one of them is active.
 *
 * In this interface, layers are numbered starting from zero.  Information
 * about a layer requires passing the layer number or "layer index".  If the
 * layer number is displayed visually it will may use 1 based numbering but
 * all internal layer references are made using zero based indexing.
 *
 */

#pragma once

class LayerModel
{
  public:

    LayerModel() {}
    ~LayerModel() {}

    // initialize from (usually) live loop state
    void initialize(MobiusLoopState* src);

    // total number of layers
    int getLayerCount();

    // the layer considered active, -1 if there are no layers
    int getActiveLayer();

    // returns true if the layer with this index does not exist
    bool isVoid(int index);

    // returns true if the layer at this index is active
    bool isActive(int index);

    // true this layer is a checkpoint
    bool isCheckpoint(int index);

    // true if this layer is a ghost
    // not normally relevant, but might be
    bool isGhost(int index);

    // return the LayerState with layer details if available
    // nullptr returned if this is a ghost layer
    MobiusLayerState* getState(int index);

  private:

    // MobiusState model object that is assumed to remain
    // valid for the lifetime of this object
    MobiusLoopState* state = nullptr;
    int totalLayers = 0;
    int activeLayer = 0;

};

/**
 * This class assists the UI by wrapping the LayerModel to provide
 * a scrolling "view" of the layers.  The view is usually smaller than
 * the layer model, but may be larger.
 *
 * The view has a length, which is the number of layers that can be displayed.
 *
 * The view has a "base" which is the first layer index that is displayed.  This
 * is normally positive but it could be negtive if the UI wishes to display layers
 * centered or right justifified within the available space.
 *
 * The view will adjust the base to ensure that the active layer is always within
 * the display range.  The view adjusts on a periodic refresh cycle to reflect changes
 * to the layer model made since the last time it was visualized.
 *
 * To prevent abrupt changes in the UI, the view must be given the last view base
 * that was used when it was rendered. The view will try to retain the same view base
 * as long as the active layer remains visible and there is a useful number of
 * surrounding inactive layers displayed to provide the user visual context.
 *
 * Adjustment to the view base is performed by the orient() method after the view
 * is initialized.  orient() will adjust the view base to ensure the active layer is
 * visible and will calculate two "loss" numbers to represent the layers that are not
 * within the view window.  Older layers outside the view are called the "preLoss"
 * and newer layers outside the view are called the "postLoss".  This can also be though
 * of as the "undo loss" and the "redo loss".
 *
 * Once the view is oriented, the UI references the layers within the view with
 * by "view index" sometimes referred to in the code as "bars" since they are displayed
 * as vertical colored rectangles with the color representing the state of the layer
 * at that position.
 *
 * Calling view methods with an index that is out of range will return zero or false.
 * 
 */
class LayerView
{
  public:

    LayerView() {}
    ~LayerView() {}

    // The layer view is initialized on every display refresh cycle
    // and will automatically orient.  The UI may adjust the orientation
    // by calling setViewBase
    void initialize(MobiusLoopState* state, int viewSize, int lastViewBase);

    // return the view base after orientation
    // not usually interesting during use, but can be interesting for diagnostic messages
    int getViewBase();
    
    // change the view base, this would be unusual as the view self-orients
    // and is normally allowed to maintain it's own orientation
    void setViewBase(int base, bool reorient = true);

    // A set of layer state accessors for the layers underneath each view bar
    // the index is a view index from zero to viewSize

    // true if this bar has no underlying layer
    bool isVoid(int index);

    // true if this bar represents the active layer
    bool isActive(int index);

    // true if this bar represents an undo layer
    bool isUndo(int index);

    // true if this bar represents a redo layer
    bool isRedo(int index);

    // true if this bar represents a ghost layer, either undo or redo
    bool isGhost(int index);

    // true if this bar represents a checkpoint layer
    bool isCheckpoint(int index);

    // the number of undo layers that were not in the view
    int getPreLoss();

    // the number of redo layers that were not in the view
    int getPostLoss();

  private:

    MobiusLoopState* state = nullptr;
    LayerModel model;

    int viewBase = 0;
    int viewSize = 0;
    int preLoss = 0;
    int postLoss = 0;
    
    void orient();
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

                  
    

    

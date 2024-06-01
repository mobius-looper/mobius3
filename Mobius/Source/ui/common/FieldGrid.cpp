/**
 * A field grid is a set of Fields that can be arranged in columns.
 * Fields must be heap allocated and ownership transfers to the FieldGrid.
 */

#include <JuceHeader.h>

#include "Field.h"
#include "FieldGrid.h"

//////////////////////////////////////////////////////////////////////
//
// FieldGrid
//
//////////////////////////////////////////////////////////////////////

FieldGrid::FieldGrid()
{
    setName("FieldGrid");
}

FieldGrid::~FieldGrid()
{
}

void FieldGrid::add(Field* f, int column)
{
    juce::OwnedArray<Field>* fieldColumn = nullptr;
    
    if (column >= columns.size()) {
        fieldColumn = new juce::OwnedArray<Field>();
        columns.set(column, fieldColumn);
    }
    else {
        fieldColumn = columns[column];
    }

    fieldColumn->add(f);
    addAndMakeVisible(f);

    // should we resize as we go or require the caller
    // to do it at the end?
}

/**
 * Add pointers to the Fields we contain to the Array.
 * This is used in cases where something needs to iterate
 * over all the Fields within a container hierarchy.
 *
 * This little traversal happens three times now.  If we keep
 * doing this either define a FieldIterator or better might
 * be just to maintain them on a single list, put the column on the Field
 * and have resized() figure it out.
 */
void FieldGrid::gatherFields(juce::Array<Field*>& fields)
{
    for (int col = 0 ; col < columns.size() ; col++) {
        juce::OwnedArray<Field>* column = columns[col];
        if (column != nullptr) {
            for (int row = 0 ; row < column->size() ; row++) {
                Field* f = (*column)[row];
                fields.add(f);
            }
        }
    }
}


/**
 * Iterate over the fields we contain and render them as Juce components.
 * 
 * This happens twice now, does it make sense to define a FieldIterator?
 *
 * Calculate the minim size and set it.  
 */
void FieldGrid::render()
{
    // make sure the Fields are rendered
    for (int col = 0 ; col < columns.size() ; col++) {
        juce::OwnedArray<Field>* column = columns[col];
        if (column != nullptr) {
            for (int row = 0 ; row < column->size() ; row++) {
                Field* f = (*column)[row];
                f->render();
            }
        }
    }

    juce::Rectangle<int> size = getMinimumSize();
    setSize(size.getWidth(), size.getHeight());
}

/**
 * Calculate the minimum size required by this grid.
 * This will become the initial size in render()
 * but may be changed by the parent.
 *
 * If given a larger size, we should try to center it
 * during layout.
 *
 * To do alignment of labels first calculate the maximum
 * label width MLW.  Then the maximum field width without
 * labels, or the maximum render width MRW.  The maximum
 * field width is then MLW + MRW.
 */
juce::Rectangle<int> FieldGrid::getMinimumSize()
{
    int maxWidth = 0;
    int maxHeight = 0;
    
    for (int col = 0 ; col < columns.size() ; col++) {
        juce::OwnedArray<Field>* column = columns[col];
        if (column != nullptr) {
            int colWidth = 0;
            int colHeight = 0;
            
            // get MLW and MRW
            int maxLabelWidth = 0;
            int maxRenderWidth = 0;
            for (int row = 0 ; row < column->size() ; row++) {
                Field* f = (*column)[row];
                if (f->getLabelWidth() > maxLabelWidth)
                  maxLabelWidth = f->getLabelWidth();
                if (f->getRenderWidth() > maxRenderWidth)
                  maxRenderWidth = f->getRenderWidth();
                // height just adds
                colHeight += f->getHeight();
            }

            int maxFieldWidth = maxLabelWidth + maxRenderWidth;
            if (maxFieldWidth > colWidth)
              colWidth = maxFieldWidth;
            
            maxWidth += colWidth;
            if (colHeight > maxHeight)
              maxHeight = colHeight;
        }
    }

    return juce::Rectangle<int> {0, 0, maxWidth, maxHeight};
}

/**
 * Layout needs to much more complicated to provide different
 * options for field label justification.  For now we are just a
 * simple vertical panel for fields and an evenly divided horizontal
 * panel for columns.  Could use Panel here?
 *
 * Column widths can follow exactly what the fields one or we can
 * evenly subdivide the available space by column.  When this is at
 * the minimum size, the effect is the same, but if we're given a larger
 * space it would space them out better.
 */
void FieldGrid::resized()
{
    int colOffset = 0;
    
    for (int col = 0 ; col < columns.size() ; col++) {
        juce::OwnedArray<Field>* column = columns[col];
        int maxWidth = 0;
        if (column != nullptr) {
            int rowOffset = 0;

            // first determine MLW
            int maxLabelWidth = 0;
            for (int row = 0 ; row < column->size() ; row++) {
                Field* f = (*column)[row];
                int w = f->getLabelWidth();
                if (w > maxLabelWidth)
                  maxLabelWidth = w;
            }

            // offset each field by column and label justification
            for (int row = 0 ; row < column->size() ; row++) {
                Field* f = (*column)[row];
                int alignOffset = maxLabelWidth - f->getLabelWidth();
                f->setTopLeftPosition(colOffset + alignOffset, rowOffset);
                rowOffset += f->getHeight();
                int fieldWidth = maxLabelWidth + f->getRenderWidth();
                if (fieldWidth > maxWidth)
                  maxWidth = f->getWidth();
            }
        }
        colOffset += maxWidth;
    }
}

void FieldGrid::paint(juce::Graphics& g)
{
    // give it an obvious background
    // need to work out  borders
    g.fillAll (juce::Colours::black);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

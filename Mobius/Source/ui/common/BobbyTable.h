/**
 * Yet another in a long line of table implementations.
 * This wraps the juce::TableListBox and provides a simpler
 * interface for dealing with them.
 *
 * There are two ways to put things in a table:
 *
 *    - provide a TableModel
 *    - proivde a list of TableRows
 *
 * 
 */

   






#pragma once

#include <JuceHeader.h>

class BobbyTable : public juce::Component
{
  public:

    BobbyTable();
    ~BobbyTable();

    /**
     * Tables have columns.
     * Ids must begin from 1.  Width may be zero in which case it defaults.
     */
    void addColumn(int id, juce::String name, int width = 0);

    /**
     * One of the rows may be highlighted.
     */
    void setSelectedRow(int r);
    int getSelectedRow();

    
    

  private:

    juce::TableListBox table;
    
    
};

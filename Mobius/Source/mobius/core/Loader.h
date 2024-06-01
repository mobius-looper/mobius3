/**
 * New class to encapsulate code related to loop/project loading.
 */

#pragma once

class Loader
{
  public:

    Loader(class Mobius* m) {
        mobius = m;
    }
    ~Loader() {}

    // it takes the Audio and puts it in a Loop
    void loadLoop(class Audio* a, int track, int loop);
    
  private:

    class Mobius* mobius = nullptr;

    class Layer* buildLayer(class Audio* audio);

};

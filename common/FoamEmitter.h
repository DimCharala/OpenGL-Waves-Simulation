#pragma once
#include "IntParticleEmitter.h"

class FoamEmitter : public IntParticleEmitter {
public:
    FoamEmitter(Drawable* model, int number);
    
    int active_particles = 0;
    void updateParticles(float time, float dt, glm::vec3 camera_pos) override;
    void createNewParticle(int index) override;
};


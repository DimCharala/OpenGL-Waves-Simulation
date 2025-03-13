#include "FoamEmitter.h"
#include <glm/glm.hpp>
#include <algorithm>
#include "model.h"
#include <iostream>


// FoamEmitter Constructor
FoamEmitter::FoamEmitter(Drawable* model, int number)
    : IntParticleEmitter(model, number) {}

// Update all foam particles
void FoamEmitter::updateParticles(float time, float dt, glm::vec3 camera_pos) {

    // Gradually increase the number of active particles up to the maximum
    if (active_particles < number_of_particles) {
        int batch = 30; // Number of particles to activate in each update
        int limit = std::min(number_of_particles - active_particles, batch);
        for (int i = 0; i < limit; i++) {
            createNewParticle(active_particles);
            active_particles++;
        }
    }
    else {
        active_particles = number_of_particles; // Ensure we don't exceed the max number of particles
    }

    for (int i = 0; i < active_particles; i++) {
        particleAttributes& particle = p_attributes[i];
        std::cout << particle.position.x <<" "<< particle.position.y << " "<< particle.position.z <<" "<< camera_pos.x << " " << camera_pos.y << " " << camera_pos.z << std::endl;

        // If the particle's life runs out or falls below a certain threshold, respawn it
        if (particle.life <= 0.0f || particle.position.y < emitter_pos.y) {
            createNewParticle(i);
        }

        // Foam particles should move along the water surface and slowly fade out
        particle.accel = glm::vec3(0.0f, 0.0f, 0.0f); // No gravity for foam
        particle.velocity *= 0.95f; // Slow down over time (simulating foam staying near the surface)

        // Update position and velocity
        particle.position += particle.velocity * dt;

        // Foam particles gradually fade over time
        particle.life -= dt * 0.1f; // Foam life decreases slowly
    }
}

// Create a new foam particle at the given index
void FoamEmitter::createNewParticle(int index) {
    particleAttributes& particle = p_attributes[index];
    std::cout << "Foam" << std:: endl;

    // Foam particles appear randomly along the water surface
    particle.position = emitter_pos + glm::vec3(
        (rand() % 100 / 100.0f - 0.5f) * 5.0f, // Random X offset
        0.0f,                                  // Y stays at water surface level
        (rand() % 100 / 100.0f - 0.5f) * 5.0f  // Random Z offset
    );

    // Low, random horizontal velocity to simulate floating foam
    particle.velocity = glm::vec3(
        (rand() % 100 / 100.0f - 0.5f) * 0.2f, // Small random X velocity
        0.0f,                                  // No vertical movement
        (rand() % 100 / 100.0f - 0.5f) * 0.2f  // Small random Z velocity
    );

    // Foam particles have a small mass and long life
    particle.mass = 0.1f;   // Lightweight foam
    particle.life = 1.0f;   // Full life when spawned
}

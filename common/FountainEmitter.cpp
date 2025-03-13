#include "FountainEmitter.h"
#include <iostream>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

FountainEmitter::FountainEmitter(Drawable* _model, int number)
    : IntParticleEmitter(_model, number) {}

// Function to update the particles over time
void FountainEmitter::updateParticles(float time, float dt, glm::vec3 camera_pos) {

    // Slowly increase the number of active particles
    if (active_particles < number_of_particles) {
        int batch = 30;
        int limit = std::min(number_of_particles - active_particles, batch);
        for (int i = 0; i < limit; i++) {
            createNewParticle(active_particles);
            active_particles++;
        }
    }
    else {
        active_particles = number_of_particles;
    }

    float adjusted_dt = dt;

    for (int i = 0; i < active_particles; i++) {
        particleAttributes& particle = p_attributes[i];

        // Check for out-of-bounds or collisions with ground
        if (particle.position.y < emitter_pos.y - 10.0f || particle.life == 0.0f || checkForCollision(particle)) {
            createNewParticle(i);
        }

        // If particle exceeds height threshold, recreate it
        if (particle.position.y > height_threshold)
            createNewParticle(i);

        // Apply gravity (downward force)
        particle.accel = glm::vec3(0.0f, -1.0f, 0.0f);  // Much lower gravity for a slower fall

        // Update particle position and velocity with scaled time step
        particle.position += particle.velocity * adjusted_dt + particle.accel * (adjusted_dt * adjusted_dt) * 0.5f;
        particle.velocity += particle.accel * adjusted_dt;

        // Apply even stronger velocity damping
        particle.velocity *= 0.80f;  // Stronger damping to reduce speed

        // Update rotation (billboarding effect)
        auto bill_rot = calculateBillboardRotationMatrix(particle.position, camera_pos);
        particle.rot_axis = glm::vec3(bill_rot.x, bill_rot.y, bill_rot.z);
        particle.rot_angle = glm::degrees(bill_rot.w);

        // Handle particle "clashing" effect: simple distance-based repulsion
        for (int j = 0; j < active_particles; j++) {
            if (i != j) {
                particleAttributes& otherParticle = p_attributes[j];
                glm::vec3 diff = particle.position - otherParticle.position;
                float dist = glm::length(diff);
                float minDist = 0.5f;  // Minimum distance between particles for clashing

                if (dist < minDist) {
                    glm::vec3 repulsion = glm::normalize(diff) * (minDist - dist) * 0.5f;
                    particle.velocity += repulsion;
                    otherParticle.velocity -= repulsion;
                }
            }
        }

        // Update particle life based on height
        particle.life = (height_threshold - particle.position.y) / (height_threshold - emitter_pos.y);
    }
}

// Function to check for ground collision
bool FountainEmitter::checkForCollision(particleAttributes& particle) {
    return particle.position.y < 0.0f;
}

// Function to create a new particle
void FountainEmitter::createNewParticle(int index) {
    particleAttributes& particle = p_attributes[index];

    // Randomize the starting position with less spread for "bubble" effect
    float spread = 0.005f;  // Reduce spread to avoid wide distribution
    particle.position = emitter_pos + glm::vec3(spread * (RAND - 0.5f), spread * RAND, spread * (RAND - 0.5f));

    // Randomize the velocity to simulate the "crashing and bouncing"
    float upwardVelocity = 0.2f;  // Lower vertical movement for bubbles
    float horizontalRange = 0.1f;  // Reduce horizontal movement range to prevent wide movement
    particle.velocity = glm::vec3(horizontalRange * (0.5f - RAND), upwardVelocity + RAND * 0.2f, horizontalRange * (0.5f - RAND));

    particle.mass = RAND + 0.5f;
    particle.rot_axis = glm::normalize(glm::vec3(1 - 2 * RAND, 1 - 2 * RAND, 1 - 2 * RAND));
    particle.accel = glm::vec3(0.0f, -1.0f, 0.0f);  // Very small gravity force for slow fall
    particle.rot_angle = RAND * 360;
    particle.life = 0.2f;  // Mark it as alive
}

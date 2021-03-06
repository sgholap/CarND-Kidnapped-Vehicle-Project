/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
    num_particles = 5;  // Set the number of particle to 5
    
	// Initialize random device engine adding Gaussian noise.
    std::random_device rd;
    std::default_random_engine gen(rd());
	
	// Initialize normal distribution with standard deviation for x, y and theta.
    std::normal_distribution<double> dist_x(x, std[0]);
    std::normal_distribution<double> dist_y(y, std[1]);
    std::normal_distribution<double> dist_theta(theta, std[2]);
    
	// Set vector to size of particle
    particles.resize(num_particles);
    weights.resize(num_particles);
	
    // Initialize particle with GPS value with randomness through Gaussian noise.
	// Intialize weight to 1.
    for (auto i = 0; i < num_particles; ++i) {
        particles[i].x = dist_x(gen);
        particles[i].y = dist_y(gen);
        particles[i].weight = 1.;
        particles[i].theta = dist_theta(gen);
        weights[i] = 1.;
    }
    is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */

   std::random_device rd;
   std::default_random_engine gen(rd());
   for (auto i = 0; i < num_particles; i++) {
        if (std::abs(yaw_rate) <= std::numeric_limits<double>::epsilon()) {
            // If yaw_rate is too small ~0. It is no turn scenario.
            particles[i].x += velocity * delta_t * std::cos(particles[i].theta);
            particles[i].y += velocity * delta_t * std::sin(particles[i].theta);
        } else {
            // If yaw_rate is non-zero.
            double thetaf = particles[i].theta + yaw_rate * delta_t;
            particles[i].x += velocity / yaw_rate * (std::sin(thetaf) - std::sin(particles[i].theta));
            particles[i].y += velocity / yaw_rate * (std::cos(particles[i].theta) - std::cos(thetaf));
            particles[i].theta = thetaf;
        }
        std::normal_distribution<double> dist_x(particles[i].x, std_pos[0]);
        std::normal_distribution<double> dist_y(particles[i].y, std_pos[1]);
        std::normal_distribution<double> dist_theta(particles[i].theta, std_pos[2]);
        particles[i].x = dist_x(gen);
        particles[i].y = dist_y(gen);
        particles[i].theta = dist_theta(gen);
   }
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
   for (auto& observation : observations)
   {
       observation.id = -1;
       double minDist = std::numeric_limits<double>::max();
       for (auto landmark : predicted)
       {
           // Find distance and find predicted landmark that is closest this observation.
		   // Update Id to nearest Id. This will be use later to update weight.
           double distance = dist(landmark.x, landmark.y, observation.x, observation.y);
           if (distance < minDist)
           {
               observation.id = landmark.id;
               minDist = distance;
           }
       }
   }
}

static double multiVariant(double x, double y, double mux, double muy, double sigmax, double sigmay)
{
    double xTerm = std::pow((x - mux), 2) / 2 / std::pow(sigmax, 2);
    double yTerm = std::pow((y - muy), 2) / 2 / std::pow(sigmay, 2);
    double constantFactor = 2* M_PI * sigmax * sigmay;
    
    return (std::exp(-(xTerm + yTerm)) / constantFactor);
}
void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */
   // Iterate over each particle
   for (auto i = 0; i < num_particles; i++) {
       particles[i].weight  = 1;
       // Sensor Range check. Check landmark is within sensor range to particle
       vector<LandmarkObs> predictedLandmark;
       for (auto landmark : map_landmarks.landmark_list)
       {
           if (dist(particles[i].x, particles[i].y, landmark.x_f, landmark.y_f) <= sensor_range)
           {
               LandmarkObs ld;
               ld.id = landmark.id_i;
               ld.x = landmark.x_f;
               ld.y = landmark.y_f;
               predictedLandmark.push_back(ld);
           }
       }

       // Transformation : Transform from car coordinate(observations) to map coordinate (particles are in map coordinate
       // using homogeneous equation.
       vector<LandmarkObs> observationsInMap(observations);

       for (auto& observation : observationsInMap)
       {
            double x = particles[i].x +
                    (observation.x * std::cos(particles[i].theta) - observation.y * std::sin(particles[i].theta));
            observation.y = particles[i].y +
                    (observation.x * std::sin(particles[i].theta) + observation.y * std::cos(particles[i].theta));
            observation.x = x;
       }
       
       //Association: Associate observation point to nearest neighbor. ( Update id of observations);
       dataAssociation(predictedLandmark, observationsInMap);
       
       // Find measurement probability for observation
       double mux = 0;
       double muy = 0;
       for (auto& observation : observationsInMap)
       {
           if ( observation.id == -1)
           {
               // If no landmark is in sensor range. What to do?
               // Ignore it.
               continue;
           }
           for (auto& ld : predictedLandmark)
           {
               if (observation.id == ld.id)
               {
                   mux = ld.x;
                   muy = ld.y;
                   break;
               }
           }
           // multiVariant distribution to update weight.
           particles[i].weight *= multiVariant(observation.x, observation.y, mux, muy, std_landmark[0], std_landmark[1]);
       }
   }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
    vector<Particle> resample_particles;
    std::random_device rd;
    std::default_random_engine gen(rd());
    weights.clear();
    for (auto i = 0; i < num_particles; i++) {
        weights.push_back(particles[i].weight);
    }

    // Implement re-sampling wheel.
    // Initialize wheel
    std::uniform_int_distribution<int> intDist(0, num_particles - 1);
    auto index = intDist(gen);
    
    // Find max element of vector weight
    double max_weight = *max_element(weights.begin(), weights.end());
    std::uniform_real_distribution<double> realDist(0.0, max_weight);
    
    double beta = 0.0;
    for (auto i = 0; i < num_particles; i++)
    {
        beta += realDist(gen) * 2.0;
        while (beta > weights[index])
        {
            beta -= weights[index];
            index = (index + 1) % num_particles;
        }
        resample_particles.push_back(particles[index]);
    }
    particles = resample_particles;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}
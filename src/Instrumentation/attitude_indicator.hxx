/*
 * SPDX-License-Identifier: CC0-1.0
 * 
 * attitude_indicator.hxx - a vacuum-powered attitude indicator.
 * Written by David Megginson, started 2002.
 * 
 * This file is in the Public Domain and comes with no warranty.
*/
#pragma once

#ifndef __cplusplus
# error This library requires C++
#endif

#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include "gyro.hxx"


/**
 * Model a vacuum-powered attitude indicator.
 *
 * Input properties:
 *
 * /instrumentation/"name"/config/tumble-flag
 * /instrumentation/"name"/serviceable
 * /instrumentation/"name"/caged-flag
 * /instrumentation/"name"/tumble-norm
 * /orientation/pitch-deg
 * /orientation/roll-deg
 * "vacuum-system"/suction-inhg
 *
 * Output properties:
 *
 * /instrumentation/"name"/indicated-pitch-deg
 * /instrumentation/"name"/indicated-roll-deg
 * /instrumentation/"name"/tumble-norm
 */
class AttitudeIndicator : public SGSubsystem
{
public:
    AttitudeIndicator ( SGPropertyNode *node );
    virtual ~AttitudeIndicator ();

    // Subsystem API.
    void bind() override;
    void init() override;
    void reinit() override;
    void unbind() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "attitude-indicator"; }

private:
    std::string _name;
    int _num;
    std::string _suction;

    Gyro _gyro;

    SGPropertyNode_ptr _tumble_flag_node;
    SGPropertyNode_ptr _caged_node;
    SGPropertyNode_ptr _tumble_node;
    SGPropertyNode_ptr _pitch_in_node;
    SGPropertyNode_ptr _roll_in_node;
    SGPropertyNode_ptr _suction_node;
    SGPropertyNode_ptr _pitch_int_node;
    SGPropertyNode_ptr _roll_int_node;
    SGPropertyNode_ptr _pitch_out_node;
    SGPropertyNode_ptr _roll_out_node;
    
    double spin_thresh;
    double max_roll_error;
    double max_pitch_error;
};

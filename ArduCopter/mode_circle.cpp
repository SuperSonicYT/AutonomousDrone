#include "Copter.h"

#if MODE_CIRCLE_ENABLED == ENABLED

bool ModeCircle::init(bool ignore_checks)
{
    pilot_yaw_override = false;
    speed_changing = false;

    pos_control->set_max_speed_accel_xy(wp_nav->get_default_speed_xy(), wp_nav->get_wp_acceleration());
    pos_control->set_correction_speed_accel_xy(wp_nav->get_default_speed_xy(), wp_nav->get_wp_acceleration());
    pos_control->set_max_speed_accel_z(-get_pilot_speed_dn(), g.pilot_speed_up, g.pilot_accel_z);
    pos_control->set_correction_speed_accel_z(-get_pilot_speed_dn(), g.pilot_speed_up, g.pilot_accel_z);

    copter.circle_nav->init();

    return true;
}

void ModeCircle::run()
{  
    pos_control->set_max_speed_accel_xy(wp_nav->get_default_speed_xy(), wp_nav->get_wp_acceleration());
    pos_control->set_max_speed_accel_z(-get_pilot_speed_dn(), g.pilot_speed_up, g.pilot_accel_z);

    float target_yaw_rate = get_pilot_desired_yaw_rate(channel_yaw->norm_input_dz());
    if (!is_zero(target_yaw_rate)) {
        pilot_yaw_override = true;
    }

    copter.circle_nav->check_param_change();

    if (!copter.failsafe.radio && copter.circle_nav->pilot_control_enabled()) {
        const float radius_current = copter.circle_nav->get_radius();
        const float pitch_stick = channel_pitch->norm_input_dz();
        const float nav_speed = copter.wp_nav->get_default_speed_xy();
        const float radius_pilot_change = (pitch_stick * nav_speed) * G_Dt;
        const float radius_new = MAX(radius_current + radius_pilot_change,0);
        
        if (!is_equal(radius_current, radius_new)) {
            copter.circle_nav->set_radius_cm(radius_new);
        }
        if (g.radio_tuning != TUNING_CIRCLE_RATE) {
            const float roll_stick = channel_roll->norm_input_dz();     

            if (is_zero(roll_stick)) {
                speed_changing = false;
            } else {
                const float rate = copter.circle_nav->get_rate();         
                const float rate_current = copter.circle_nav->get_rate_current();
                const float rate_pilot_change = (roll_stick * G_Dt);  
                float rate_new = rate_current;                      
                if (is_positive(rate)) {
                    rate_new = constrain_float(rate_current + rate_pilot_change, 0, 90);

                } else if (is_negative(rate)) {
                    rate_new = constrain_float(rate_current + rate_pilot_change, -90, 0);

                } else if (is_zero(rate) && !speed_changing) {
                    rate_new = rate_pilot_change;
                }

                speed_changing = true;
                copter.circle_nav->set_rate(rate_new);
            }
        }
    }
    float target_climb_rate = get_pilot_desired_climb_rate(channel_throttle->get_control_in());

    if (is_disarmed_or_landed()) {
        make_safe_ground_handling();
        return;
    }

    motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

    copter.surface_tracking.update_surface_offset();

    copter.failsafe_terrain_set_status(copter.circle_nav->update(target_climb_rate));

    if (pilot_yaw_override) {
        attitude_control->input_thrust_vector_rate_heading(copter.circle_nav->get_thrust_vector(), target_yaw_rate);
    } else {
        attitude_control->input_thrust_vector_heading(copter.circle_nav->get_thrust_vector(), copter.circle_nav->get_yaw());
    }
    pos_control->update_z_controller();
}

uint32_t ModeCircle::wp_distance() const
{
    return copter.circle_nav->get_distance_to_target();
}

int32_t ModeCircle::wp_bearing() const
{
    return copter.circle_nav->get_bearing_to_target();
}

#endif

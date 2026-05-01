#ifndef MAIN_A0090_SERVO_MOTOR_H_
#define MAIN_A0090_SERVO_MOTOR_H_

/**
 * Initializes all motores per hand
 */
void a0090_servor_motor_init(void);


/**
 * setting fingers postion
 */
void a0090_servor_motor_set_finger(int finger_location, int finger_angle);


#endif /* MAIN_A0090_SERVO_MOTOR_H_ */

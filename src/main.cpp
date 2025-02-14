#include "main.h"

//----------------------------------------------------------------------------------------------------------------DRIVETRAIN CONFIG-----------------------------------------------------------------------------------------------------------------

// left motor group
pros::MotorGroup left_motor_group({-1, -4, -5}, pros::MotorGears::blue);
// right motor group
pros::MotorGroup right_motor_group({7, 8, 9}, pros::MotorGears::blue);

// drivetrain settings
lemlib::Drivetrain drivetrain(&left_motor_group, // left motor group
                              &right_motor_group, // right motor group
                              10, // 10 inch track width
                              lemlib::Omniwheel::NEW_4, // using new 4" omnis
                              360, // drivetrain rpm is 360
                              2 // horizontal drift is 2 (for now)
);
//SUBSYTEMS and their variables
inline pros::Motor intakeMotors {-3};//intake
double intakeSpeed = 600; //defualts to full speed

inline pros::ADIDigitalOut clampCylinder('H');//mogo clamp
bool clampState = false;

inline pros::ADIDigitalOut leftSweeperCylinder('G');//left sweeper
bool leftSweeperState = false;

inline pros::Optical colorSensor(19); //color sensor

//code for arm config
const int heights[3] = {2, 90, 700};//different lift heights --- tunable
int positionIndex = 0;
auto armControl = AsyncPosControllerBuilder().withMotor({20, -11}).build(); //cookn up smth vicious


//----------------------------------------------------------------------------------------------------------------ODOMETRY CONFIG-----------------------------------------------------------------------------------------------------------------

// imu
pros::Imu imu(2);
// horizontal tracking wheel encoder
pros::Rotation horizontal_encoder(6);
// horizontal tracking wheel
lemlib::TrackingWheel horizontal_tracking_wheel(&horizontal_encoder, lemlib::Omniwheel::NEW_2, 0.6875);

// odometry settings
lemlib::OdomSensors sensors(nullptr, // vertical tracking wheel 1, set to null
                            nullptr, // vertical tracking wheel 2, set to nullptr as we are using IMEs
                            &horizontal_tracking_wheel, // horizontal tracking wheel 1
                            nullptr, // horizontal tracking wheel 2, set to nullptr as we don't have a second one
                            &imu // inertial sensor
);

//----------------------------------------------------------------------------------------------------------------PID CONFIG-----------------------------------------------------------------------------------------------------------------

// lateral PID controller
lemlib::ControllerSettings lateral_controller(10, // proportional gain (kP)
                                              0, // integral gain (kI)
                                              3, // derivative gain (kD)
                                              3, // anti windup
                                              1, // small error range, in inches
                                              100, // small error range timeout, in milliseconds
                                              3, // large error range, in inches
                                              500, // large error range timeout, in milliseconds
                                              20 // maximum acceleration (slew)
);

// angular PID controller
lemlib::ControllerSettings angular_controller(2, // proportional gain (kP)
                                              0, // integral gain (kI)
                                              10, // derivative gain (kD)
                                              3, // anti windup
                                              1, // small error range, in degrees
                                              100, // small error range timeout, in milliseconds
                                              3, // large error range, in degrees
                                              500, // large error range timeout, in milliseconds
                                              0 // maximum acceleration (slew)
);

//-------------------------------------------------------------------------------------------------------------CHASSIS CONFIG---------------------------------------------------------------------------------------------------------------

// input curve for throttle input during driver control
lemlib::ExpoDriveCurve throttle_curve(3, // joystick deadband out of 127
                                     10, // minimum output where drivetrain will move out of 127
                                     1.019 // expo curve gain
);

// input curve for steer input during driver control
lemlib::ExpoDriveCurve steer_curve(3, // joystick deadband out of 127
                                  10, // minimum output where drivetrain will move out of 127
                                  1.019 // expo curve gain
);

// create the chassis
lemlib::Chassis chassis(drivetrain, // drivetrain settings
                        lateral_controller, // lateral PID settings
                        angular_controller, // angular PID settings
                        sensors, // odometry sensors
						&throttle_curve,
						&steer_curve
);

// initialize function. Runs on program startup
void initialize() {
    pros::lcd::initialize(); // initialize brain screen
    chassis.calibrate(); // calibrate sensors
    // print position to brain screen
    pros::Task screen_task([&]() {
        while (true) {
            // print robot location to the brain screen
            pros::lcd::print(0, "X: %f", chassis.getPose().x); // x
            pros::lcd::print(1, "Y: %f", chassis.getPose().y); // y
            pros::lcd::print(2, "Theta: %f", chassis.getPose().theta); // heading
            // delay to save resources
            pros::delay(20);
        }
    });
}

/**
 * Runs while the robot is in the disabled state of Field Management System or
 * the VEX Competition Switch, following either autonomous or opcontrol. When
 * the robot is enabled, this task will exit.
 */
void disabled() {}

/**
 * Runs after initialize(), and before autonomous when connected to the Field
 * Management System or the VEX Competition Switch. This is intended for
 * competition-specific initialization routines, such as an autonomous selector
 * on the LCD.
 *
 * This task will exit when the robot is enabled and autonomous or opcontrol
 * starts.
 */
void competition_initialize() {}

/**
 * Runs the user autonomous code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the autonomous
 * mode. Aylternatively, this function may be called in initialize or opcontrol
 * for non-competition testing purposes.
 *
 * If the robot is disabled or communications is lost, the autonomous task
 * will be stopped. Re-enabling the robot will restart the task, not re-start it
 * from where it left off.
 */
void autonomous() {
    int defualtTimeout = 1000;

    chassis.setPose(65.17, 12.5, 180);//start pos -- 0 degrees is facing the cage
    chassis.turnToHeading(134, defualtTimeout);//turn to allaince stake
    pros::delay(75);//this is the time between the start of auton and the start of the wall stakes motion - tunable
    armControl->setTarget(heights[2]);//raising arm
    pros::delay(125);//time to let arm finish motion - tunable
    chassis.moveToPose(68.17, 12.5, 0, defualtTimeout);

}
/*  
 * Runs the operator control code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the operator
 * control mode. 
 *
 * If no competition control is connected, this function will run immediately
 * following initialize().
 *
 * If the robot is disabled or communications is lost, the
 * operator control task will be stopped. Re-enabling the robot will restart the
 * task, not resume it from where it left off.
 */
pros::Controller controller(pros::E_CONTROLLER_MASTER);

void opcontrol() {
    
    armControl->setMaxVelocity(165);//tuneable
    // loop forever
    while (true) {
        // get left y and right x positions
        int leftY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
        int rightX = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);

        // move the robot
        chassis.curvature(leftY, rightX);

        // delay to save resources
        pros::delay(25);

        if (controller.get_digital(DIGITAL_R1))
            intakeMotors.move_velocity(intakeSpeed);
        else if (controller.get_digital(DIGITAL_R2))
            intakeMotors.move_velocity(-intakeSpeed);
        else 
            intakeMotors.move_velocity(0);
        if (controller.get_digital_new_press(DIGITAL_Y))
        {
            clampCylinder.set_value(!clampState); 
            clampState = !clampState;
        }
        if (controller.get_digital_new_press(DIGITAL_LEFT))
        {
            leftSweeperCylinder.set_value(!leftSweeperState); 
            leftSweeperState = !leftSweeperState;
        }
        if (controller.get_digital_new_press(DIGITAL_L1) && positionIndex != 2){
            intakeSpeed = 600;
            positionIndex++;
            armControl->setTarget(heights[positionIndex]);//raising arm 1 state
        }
        if (controller.get_digital_new_press(DIGITAL_L2) && positionIndex != 0){
            positionIndex--; 
            armControl->setTarget(heights[positionIndex]);//dropping arm 1 state
        }
    }
}
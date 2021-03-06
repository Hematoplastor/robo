/**
 * @author Nicolas Bredeche <nicolas.bredeche@upmc.fr>
 */


#include "MoveSwap/include/MoveSwapController.h"

// Sensor Id
// (L)eft, (R)ight, (B)ack, (F)ront
// FFFL is a front sensor, pointing very slightly to the left (15°).
#define SENSOR_L 0
#define SENSOR_FL 1
#define SENSOR_FFL 2
#define SENSOR_FFFL 3
#define SENSOR_F 4
#define SENSOR_FFFR 5
#define SENSOR_FFR 6
#define SENSOR_FR 7
#define SENSOR_R 8
#define SENSOR_BR 9
#define SENSOR_B 10
#define SENSOR_BL 11

MoveSwapController::MoveSwapController( RobotWorldModel *__wm ) : Controller ( __wm )
{
    // if ( _wm->_cameraSensorsNb != 12 )
    // {
    //     std::cerr << "[CRITICAL] This project assumes robot specifications with 12 sensors (current specs: " << _wm->_cameraSensorsNb << "). STOP.\n";
    //     exit(-1);
    // }
    
    _isAttracted = ranf()>MoveSwapSharedData::gDefSwapRate;
}

MoveSwapController::~MoveSwapController()
{
	// nothing to do.
}

void MoveSwapController::reset()
{
	// nothing to do.
}

void MoveSwapController::step()
{
    if( gWorld->getIterations() > 5 ) // ( _wm->getId() < gNbOfRobots/9 ) // the others are just replicas of the ones in the center
    {
        // SOME PARAMETERS:
        double balance = MoveSwapSharedData::gBalance; // bias towards attraction if >1
        double dSwapRate = MoveSwapSharedData::gDefSwapRate; // probability at each step that a node chooses random behavior
        double sSwapRate = MoveSwapSharedData::gSpecSwapRate; // probability at each step that a node chooses random behavior
        double errorRate = MoveSwapSharedData::gErrorRate; // probability at each step that a node doesn't act according to behavior
        double acceptance = MoveSwapSharedData::gAcceptance; //.5 * (1+_isAttracted); // probability to take a neighbor into account
        int center_x = MoveSwapSharedData::gCenterX, center_y = MoveSwapSharedData::gCenterY; // position of energy-giving center
        int energyRadius = MoveSwapSharedData::gEnergyRadius; //200
        bool listeningState = MoveSwapSharedData::gListeningState;

        double invBal = 1/balance;
        double total = balance + invBal;

        //_wm->_desiredTranslationalValue = 1;
            
        // UPDATE BEHAVIOR:

        // decide value to apply for swapRate depending on position
        double swapRate = dSwapRate;
        //double sqDistToCenter = pow(center_x - _wm->_xReal, 2) + pow(center_y - _wm->_yReal, 2);
        double sqDistToCenter = std::max(pow(center_x - _wm->_xReal, 2) , pow(center_y - _wm->_yReal, 2));
        if( sqDistToCenter < pow(energyRadius, 2) )
            swapRate += (sSwapRate - dSwapRate) * (1- sqrt( sqDistToCenter ) / energyRadius);
            //swapRate = sSwapRate;

        // listen to neighbors with weight 2, to oneself with weight 4, and add a noise of weight 1
        double consensus = total * 2 * _isAttracted + rand()%2 - (3 * invBal);
        for(int i = 0; i < _wm->_cameraSensorsNb; i++)
        {
            int targetIndex = _wm->getObjectIdFromCameraSensor(i);
            if ( targetIndex >= gRobotIndexStartOffset && targetIndex < gRobotIndexStartOffset+gNbOfRobots )   // sensor ray bumped into a robot : communication is possible
            {
                targetIndex = targetIndex - gRobotIndexStartOffset; // convert image registering index into robot id.
                
                MoveSwapController* targetRobotController = dynamic_cast<MoveSwapController*>(gWorld->getRobot(targetIndex)->getController());
                
                if ( ! targetRobotController )
                {
                    std::cerr << "Error from robot " << _wm->getId() << " : the observer of robot " << targetIndex << " is not compatible." << std::endl;
                    exit(-1);
                }
                double dist = _wm->getNormalizedDistanceValueFromCameraSensor( i );
                if ( ( (double)(rand()%1000))/1000.0 < acceptance
                    && ( !(gEnergyLevel && listeningState) || (dist) * gEnergyMax < gWorld->getRobot(targetIndex)->getWorldModel()->getEnergyLevel() ) )
                    consensus += total * 2 * targetRobotController->_isAttracted - 2 * invBal;
            }
        }
        _isAttracted = ( consensus >= 0 );

        // pure random mutation
        if ( ( (double)(rand()%1000))/1000.0 < swapRate ) 
            _isAttracted = rand()%2;

// if( sqDistToCenter < pow(energyRadius, 2) )
//             //swapRate += (sSwapRate - dSwapRate) * sqrt( sqDistToCenter ) / energyRadius;
//             _isAttracted = true;



        bool __isAttracted = _isAttracted;
        if ( ( (double)(rand()%100))/100.0 < errorRate )
            __isAttracted = !__isAttracted;






        // TAKE DECISION ACCORDING TO BEHAVIOR

        if(__isAttracted){

            _wm->_desiredTranslationalValue = MoveSwapSharedData::gBiasSpeedDelta;

            // double minDist = 1;
            // int nbNeighbours = 0;
            int nbAttNeighbours = 0;
            double avgCenterOfMass_X = 0;
            double avgCenterOfMass_Y = 0;
            // int minIdx = 0;

            // compute local center of mass and min distance to detected neighbors in attractive mode
            for( int i = 0 ; i < _wm->_cameraSensorsNb ; i++)
            {
                int targetRawIndex = _wm->getObjectIdFromCameraSensor(i);
                
                if ( targetRawIndex >= gRobotIndexStartOffset )   // sensor ray bumped into a robot -- WARNING: one (very close) neighbor can be seen by more than one sensor (in the following, we dont care if this happen, though it is not correct to do so)
                {
                    int targetRobotIndex = targetRawIndex - gRobotIndexStartOffset; // convert image registering index into robot id.
                    
                    //nbNeighbours++;
                    
                    double dist = _wm->getNormalizedDistanceValueFromCameraSensor( i );
                    // if(minDist > dist){
                        // minIdx = targetRobotIndex;
                        // minDist = dist;
                    // }

                    // eventually only listen to energetic or close neighbors
                    if( !(gEnergyLevel && listeningState) || /*dynamic_cast<MoveSwapController*>(gWorld->getRobot(targetRobotIndex)->getController())->_isAttracted){//} &&*/ dist * gEnergyMax < gWorld->getRobot(targetRobotIndex)->getWorldModel()->getEnergyLevel() ){
                        nbAttNeighbours ++;
                        // update average center of mass
                        avgCenterOfMass_X = avgCenterOfMass_X + (gWorld->getRobot(targetRobotIndex)->getWorldModel()->_xReal);
                        avgCenterOfMass_Y = avgCenterOfMass_Y + (gWorld->getRobot(targetRobotIndex)->getWorldModel()->_yReal);   
                    }
                }
            }
            if (nbAttNeighbours != 0){
                avgCenterOfMass_X = avgCenterOfMass_X / nbAttNeighbours;
                avgCenterOfMass_Y = avgCenterOfMass_Y / nbAttNeighbours;
            }
            double y = avgCenterOfMass_Y - (_wm->_yReal);// coordinates of center of mass wtr. to robot
            double x = avgCenterOfMass_X - (_wm->_xReal);
            // int phase = floor(minDist * 255);

            // _wm->updateLandmarkSensor( 0 );
            if(nbAttNeighbours == 0)
            {
                _wm->_desiredRotationalVelocity = (double)(((rand()%(int)(2 * gMaxRotationalSpeed)))-gMaxRotationalSpeed);
            }
            else
            {
                moveTowards(x,y);
                // _wm->_desiredTranslationalValue = sqrt(y*y + x*x) * elasticity + 1; // doesn't change global behavior for small noise
                // _wm->_desiredTranslationalValue = nbAttNeighbours;
            }
           

        }
        else{
            _wm->_desiredRotationalVelocity = (double)(((rand()%(int)(2 * gMaxRotationalSpeed)))-gMaxRotationalSpeed);
            _wm->_desiredTranslationalValue = 1;
            // moveTowards(-x, -y);
        }
        // std::cout << "[DEBUG] Robot #" << _wm->getId() << " : behavior is " << _isAttracted << " and direction " << _wm->_desiredRotationalVelocity << ".\n";
        



        // UPDATE ENERGY
        if(gEnergyLevel)
            _wm->setEnergyLevel( _wm->getEnergyLevel() + 2 * ( pow(_wm->_xReal - center_x, 2) + pow(_wm->_yReal - center_y, 2) < pow(energyRadius,2) ) - 1 );


//TODO put in separate function fpdfPD
    /*    int fpdnbNeighbours = 0;
        int fpdarea = _wm->_cameraSensorsNb; // proxy for area around center
        for( int i = 0 ; i < _wm->_cameraSensorsNb ; i++)
        {
            int fpdtargetRawIndex = _wm->getObjectIdFromCameraSensor(i);
            if ( fpdtargetRawIndex >= gRobotIndexStartOffset )   // sensor ray bumped into a robot -- WARNING: one (very close) neighbor can be seen by more than one sensor (in the following, we dont care if this happen, though it is not correct to do so)
            {
                int fpdtargetRobotIndex = fpdtargetRawIndex - gRobotIndexStartOffset; // convert image registering index into robot id.
                fpdnbNeighbours ++;
                double fpddist = _wm->getNormalizedDistanceValueFromCameraSensor( i );
                fpdarea += (fpddist - 1);
            }
        }
        double fpd = (1. + fpdnbNeighbours) / fpdarea;
*/


        // UPDATE LED VALUES
        if(gDisplayMode==0){
        // (_wm->getLandmarkDistanceValue()<1)*255
            _wm->setRobotLED_colorValues(!_isAttracted*255,_isAttracted*255,1000*swapRate);//(_wm->getEnergyLevel())*255 / gEnergyMax
  //          _wm->setRobotLED_colorValues((_wm->getGroupId()>0)*255,(_wm->getGroupId()>0)*255,(gNumberOfRobotGroups-_wm->getGroupId())*255/gNumberOfRobotGroups);
  
        }
        
      //  monitorSensoryInformation();
    }
    else{
        _wm->_desiredRotationalVelocity = 0;
        _wm->_desiredTranslationalValue = 0;
        _wm->setGroupId(0);

        // RobotWorldModel* __wm = gWorld->getRobot( _wm->getId() % (gNbOfRobots/9) )->getWorldModel();
        // _wm->_desiredRotationalVelocity = __wm->_desiredRotationalVelocity;
        // _wm->_desiredTranslationalValue = __wm->_desiredTranslationalValue;
        
        // _isAttracted = (dynamic_cast<MoveSwapController*>(gWorld->getRobot( _wm->getId() % (gNbOfRobots/9) )->getController()))->_isAttracted;
        // // _wm->setEnergyLevel(__wm->getEnergyLevel());
        // int attractedness = 100 * (2-_isAttracted);
        // // int energy = 200 * (gEnergyMax - _wm->getEnergyLevel()) / gEnergyMax;
        // // _wm->setRobotLED_colorValues( energy, energy, energy );
    }

    // std::cout << "Iter " << gWorld->getIterations() << ":\tRobot " << _wm->getId() << " at [" << _wm->_xReal << ", " << _wm->_yReal << "]->" << _wm->_desiredRotationalVelocity << " + " << _wm->_agentAbsoluteOrientation << "\n";

}



void MoveSwapController::moveTowards(double x, double y){
    // if no privilegied direction : randomWalk
    if ( y==0 && x==0 )
        _wm->_desiredRotationalVelocity = (double)(((rand()%(int)(2*gMaxRotationalSpeed)))-gMaxRotationalSpeed);
    // move according to the quadrant it is in
    else
    {
        double slack = (ranf() - 0.5) * MoveSwapSharedData::gAngleFuzziness;
        double angle = atan2(y,x) * 180 / M_PI - _wm->_agentAbsoluteOrientation + slack;
        if (angle>=180) angle -= 360;
        if (angle<-180) angle += 360;
        if (angle>=180) angle -= 360;
        if (angle<-180) angle += 360;
        //gLogManager->write(std::to_string(angle)+"\t"+std::to_string(0)+"\n");
        _wm->_desiredRotationalVelocity = angle;
    }
}

void MoveSwapController::monitorSensoryInformation()
{
    // Note that this code is executed only for the agent which is "on focus". By default, this means agent #0.
    // When window mode:
    //      To show which agent has the focus, press 'F'
    //      To cycle through agents, press <tab> (or shift+<tab>)
    
    
    if ( gVerbose && gDisplayMode == 0 && gRobotIndexFocus == _wm->getId() )
    {
        
        std::cout << "=-= Robot #" << _wm->getId() << " : STARTING monitoring sensory information at iteration #" << gWorld->getIterations() << ".\n";
        
        // Rotational and translational speed, agent orientation wrt. upwards
        //      - *actual* and *desired* translational/rotational values are very different
        //          - actual values is what the robot is actually doing (this is measured)
        //          - desired values are set by the controller (this is set and the hardware controller tries to match it)
        //          - rational: you may ask for something (e.g. max speed) but physics and electronics may be limited
        //          - typical example: when going for max speed, the robot cannot instantaneously go at max speed.
        //      - agent orientation acts as a compass with respect the y-axis, similar to a magnetic compass where north is upward
        
        double srcOrientation = _wm->_agentAbsoluteOrientation;
        
        std::cout << "Agent orientation: " << std::setw(4) << srcOrientation << "° wrt North (ie. upwards).\n";
        
        std::cout << "Agent desired translational speed: " << _wm->_desiredTranslationalValue << std::endl;
        std::cout << "Agent desired rotational speed: " << std::setw(4) << _wm->_desiredRotationalVelocity << std::endl;
        
        std::cout << "Agent actual translational speed: " << _wm->_actualTranslationalValue << std::endl;
        std::cout << "Agent actual rotational speed: " << std::setw(4) << _wm->_actualRotationalVelocity << std::endl;

        // Landmarks
        //      - landmarks are invisible and intangible object that can act as magnetic point of interest.
        //      - usually used by providing direction and orientation wrt. the robot
        //      - example of use: magnetic compass, detection of direction to specific zone, etc.
        //      - closest landmark can be computed automatically (see below, last part)
        
        if ( gNbOfLandmarks > 0 )
        {
            // All landmarks (if any)
            
            for ( size_t i = 0 ; i != gNbOfLandmarks ; i++ )
            {
                Point2d posRobot(_wm->getXReal(),_wm->getYReal());
                Point2d closestPoint;
                
                double distanceToLandmark = getEuclidianDistance (posRobot,gLandmarks[0]->getPosition());
                double diffAngleToClosestLandmark = getAngleToTarget(posRobot,_wm->_agentAbsoluteOrientation,gLandmarks[i]->getPosition());
                
                std::cout << "Landmark " << i << " is at distance " << distanceToLandmark << ", angle = " << diffAngleToClosestLandmark << "\n";
            }
            
            // Closest landmark (in any)
            
            _wm->updateLandmarkSensor();
            std::cout << "Closest landmark is at distance " << std::setw(6) << _wm->getLandmarkDistanceValue() << ", angle = " << std::setw(6) << _wm->getLandmarkDistanceValue() << "\n";
        }
        else
            std::cout << "No landmark.\n";
        
        // Energy (unused)
        //      - in this code: energy is ignored.
        //      - battery level can be used in some experiment. Impact of battery level is to be managed locally.
        //      - EnergyRequest can be used in some experiment with specific energy point where only part of energy can harvest.
        
        std::cout << "Battery-level  : " << std::setw(6) << _wm->getEnergyLevel() << "\n"; // useless here
        std::cout << "Energy Requested (if asked) : " << std::setw(4) << _wm->getEnergyRequestValue() << "" << std::endl;
        
        // Floor sensor
        //      - read from gFootprintImage (see gFootprintImageFilename file)
        //      - typical use: gradient written on the floor, region marker. Could also be used for pheronome.
        
        double floorSensor_redValue = (double)_wm->getGroundSensor_redValue()/255.0;
        double floorSensor_greenValue = (double)_wm->getGroundSensor_greenValue()/255.0;
        double floorSensor_blueValue = (double)_wm->getGroundSensor_blueValue()/255.0;
        
        std::cout << "Floor sensor values: red=" << std::setw(4) << floorSensor_redValue << " ; green=" << std::setw(4) << floorSensor_greenValue << " ; blue=" << std::setw(4) << floorSensor_blueValue << ".\n";
        
        // LED sensor (on the robot)
        //      -
        
        double LED_redValue = (double)_wm->getRobotLED_redValue()/255.0;
        double LED_greenValue = (double)_wm->getRobotLED_greenValue()/255.0;
        double LED_blueValue = (double)_wm->getRobotLED_blueValue()/255.0;
        
        std::cout << "LED values: red=" << std::setw(4) << LED_redValue << " ; green=" << std::setw(4) << LED_greenValue << " ; blue=" << std::setw(4) << LED_blueValue << ".\n";
        
        // Camera/distance sensors -- provide: distance to obstacle, obstacle type, orientation (if robot)
        // REMARKS:
        //      - distance sensors are actually camera rays, and provides extended information:
        //          - distance to contact
        //          - type of contact (walls, objects, or robots)
        //          - if robot: group number, LED values, absolute orientation (from which one can compute relative orientation)
        //      - Objects and walls are different.
        //          - Walls are fixed, and loaded from gEnvironmentImage (see gEnvironmentImageFilename file)
        //          - There are several types of objects. Check children of object PhysicalObject.
        //          - Note that PhysicalObjects can have a tangible part (see gEnvironmentImage) and an intangible part (see gFootprintImage. The intangible part can be spotted with the floorSensor.
        //          - Some PhysicalObject are active and react to the robot actions (e.g. disappear, give energy, ...)
        //          - examples of use of a PhysicalObject:
        //              - a tangible object onto which the robot can crash. It is seen through distance sensors.
        //              - an intangible object onto which the robot can walk upon. It is seen through the floor sensor.
        //              - a mix of the two.
        
        for(int i  = 0; i < _wm->_cameraSensorsNb; i++)
        {
            double distance = _wm->getDistanceValueFromCameraSensor(i);
            // double distanceNormalized = _wm->getDistanceValueFromCameraSensor(i) / _wm->getCameraSensorMaximumDistanceValue(i); // Similar to _wm->getNormalizedDistanceValueFromCameraSensor(i); -- unused here

            int objectId = _wm->getObjectIdFromCameraSensor(i);
            
            std::cout << "Sensor #" << i << ":";
            
            if ( PhysicalObject::isInstanceOf(objectId) ) // sensor touched an object. What type? (could be GateObject, SwitchObject, RoundObject, ... -- check descendants of PhysicalObject class)
            {
                int nbOfTypes = PhysicalObjectFactory::getNbOfTypes();
                for ( int i = 0 ; i != nbOfTypes ; i++ )
                {
                    if ( i == gPhysicalObjects[objectId - gPhysicalObjectIndexStartOffset]->getType() )
                    {
                        std::cout << "object of type " << i << " detected\n";
                        break;
                    }
                }
            }
            else
            {
                if ( Agent::isInstanceOf(objectId) )
                {
                    int targetRobotId = objectId-gRobotIndexStartOffset;

                    std::cout << " touched robot #" << gWorld->getRobot(targetRobotId) << ", at distance " << std::setw(4) << distance << ".\n";
                    
                    // Distance to target , orientation wrt target, target absolute orientation, target LED values
                    // Distance to target is approximated through sensor ray length before contact.
                    
                    double tgtOrientation = gWorld->getRobot(targetRobotId)->getWorldModel()->_agentAbsoluteOrientation;
                    double delta_orientation = - ( srcOrientation - tgtOrientation );
                    if ( delta_orientation >= 180.0 )
                        delta_orientation = - ( 360.0 - delta_orientation );
                    else
                        if ( delta_orientation <= -180.0 )
                            delta_orientation = - ( - 360.0 - delta_orientation );
                    
                    std::cout << "\trelative orientation wrt target robot is " <<std::setw(4) << delta_orientation/180.0 << "\n";
                    std::cout << "\tabsolute orientation of target robot is  " <<std::setw(4) << tgtOrientation << "\n";
                    
                    // same group? -- unusued as of Oct. 2015
                    
                    if ( gWorld->getRobot(targetRobotId)->getWorldModel()->getGroupId() == _wm->getGroupId() )
                        std::cout << "\trobots are from the same group.\n";
                    else
                        std::cout << "\trobots are from different group.\n";
                    
                    // LED values of other robot (can be used for communication)
                    
                    double tgt_LED_redValue = (double)_wm->getRobotLED_redValue()/255.0;
                    double tgt_LED_greenValue = (double)_wm->getRobotLED_greenValue()/255.0;
                    double tgt_LED_blueValue = (double)_wm->getRobotLED_blueValue()/255.0;
                    
                    std::cout << "\tLED values: R=" << tgt_LED_redValue << ", G=" << tgt_LED_greenValue << ", B=" << tgt_LED_blueValue << "\n";
                    
                }
                else
                {
                    // input: wall or empty?
                    if ( objectId >= 0 && objectId < gPhysicalObjectIndexStartOffset ) // not empty, but cannot be identified: this is a wall.
                    {
                        std::cout << " touched an unindentified obstacle (probably a wall, id=" << objectId << "), at distance " << std::setw(4) << distance << ".\n";
                    }
                    else
                        std::cout << " nothing (id="<< objectId << "). Returns maximum value (" << std::setw(4) << distance << ")\n";
                }
            }
        }
        std::cout << "=-= Robot #" << _wm->getId() << " : STOPPING monitoring sensory information\n";
    }
    
}

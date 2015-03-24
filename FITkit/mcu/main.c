/*******************************************************************************
   main: Program for controlling plotter type device with stepper motors.
   Author(s): Ivan Sevcik <xsevci50 AT stud.fit.vutbr.cz>
*******************************************************************************/

#include <fitkitlib.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <float.h>
#include "demo.h"
#include "hilbert.h"

// Use pins 0-3 from Port 6
#define MOTOR_X_PIN_OFFSET 0
#define MOTOR_X_PIN_MASK 0x0F
#define MOTOR_X_PORT P6OUT
#define MOTOR_X_PORT_DIR P6DIR
// Use pin 0 from Port 4
#define TOGGLE_X_MASK 0x01
#define TOGGLE_X_PORT P4IN
#define TOGGLE_X_PORT_DIR P4DIR
// Use pins 4-7 from Port 6
#define MOTOR_Y_PIN_OFFSET 4
#define MOTOR_Y_PIN_MASK 0xF0
#define MOTOR_Y_PORT P6OUT
#define MOTOR_Y_PORT_DIR P6DIR
// Use pin 1 from Port 4
#define TOGGLE_Y_MASK 0x02
#define TOGGLE_Y_PORT P4IN
#define TOGGLE_Y_PORT_DIR P4DIR
// Use pin 2 from Port 4
#define PEN_MASK 0x04
#define PEN_PORT P4OUT
#define PEN_PORT_DIR P4DIR

#define MOTOR_X 0
#define MOTOR_Y 1
#define MOTOR_MASK 1
#define MOTOR_FORWARD 0
#define MOTOR_BACKWARD 2
#define MOTOR_DIR_MASK 2

#define IN_DRAWING_AREA 0
#define BEFORE_DRAWING_AREA 1
#define AFTER_DRAWING_AREA 2

// Constants for converting to real world unit
#define MOTOR_X_STEP_MM 0.1
#define MOTOR_Y_STEP_MM 0.12125
// INTERNAL_STEP_MM == min(MOTOR_X_STEP_MM, MOTOR_Y_STEP_MM)
#define INTERNAL_STEP_MM 0.1

#define motorPhasesCount 8
const uint8_t motorPhases[motorPhasesCount] = {0x1, 0x5, 0x4, 0x6, 0x2, 0xA, 0x8, 0x9};

#define STATE_FINISHED 0
#define STATE_MOVING 1
#define STATE_CUTTING 2

#define DRAWING_FREE 0
#define DRAWING_LINE 1
#define DRAWING_CIRCLE 2

#define DRAWING_COMPLEX_FREE 0
#define DRAWING_COMPLEX_DEMO 1
#define DRAWING_COMPLEX_HILBERT 2

#define OPERATION_IN_PROGRESS 0
#define OPERATION_FINISHED 1

#define PEN_UP 0
#define PEN_DOWN 1

#define DELAY 4
#define IDLE_TIME 2000

typedef struct LineContextStruct
{
    int32_t x1, y1, x2, y2;
    int32_t dx, dy, P1, P2, P, x, y, ystep;
    uint8_t makeSwap, leftRight, state;
} LineContext;

typedef struct CircleContextStruct
{
    int32_t x, R, sx, sy, i, j;
    uint8_t state, xGrow;
} CircleContext;

typedef union DrawingContextUnion
{
    LineContext lc;
    CircleContext cc;
} DrawingContext;

typedef struct DemoContextStruct
{
    int32_t idx;
} DemoContext;

typedef struct HilbertContextStruct
{
    int32_t n, length;
    int32_t startX, startY;
    int32_t idx;
    int32_t stepsPerLine;
} HilbertContext;

typedef union ComplexDrawingContextUnion
{
    DemoContext dc;
    HilbertContext hc;
} ComplexDrawingContext;

// Internal head position used by algorithms
int32_t internalHeadX = 0;
int32_t internalHeadY = 0;
// Real position of head in steps of each motor
int32_t realHeadX = 0;
int32_t realHeadY = 0;
// Area of head
uint8_t headXArea = IN_DRAWING_AREA;
uint8_t headYArea = IN_DRAWING_AREA;
// State of pen
uint8_t penState = PEN_UP;

uint8_t currentDrawing = DRAWING_FREE;
DrawingContext currentContext;
uint8_t currentComplexDrawing = DRAWING_COMPLEX_FREE;
ComplexDrawingContext currentComplexContext;

void swap (int32_t *a, int32_t *b)
{
    int32_t t = *a;
    *a = *b;
    *b = t;
}

int32_t m_abs_int (int32_t a)
{
    return a >= 0 ? a : -a;
}

double m_abs_dbl (double a)
{
    return a >= 0 ? a : -a;
}

int32_t m_round (double a)
{
    return (int32_t)(a + 0.5);
}

uint8_t m_equal(double x, double y)
{
    double absX = m_abs_dbl(x), absY = m_abs_dbl(y);
    return m_abs_dbl(x-y) <= ((absX < absY ? absX : absY) * DBL_EPSILON);
}

//Pouzita takzvana babylonska metoda, odvoditelna z Newtonovej.
//Velmi dobre konverguje - pocet platnych cislic sa v kazdom kroku
//priblizne zdvojnasobuje
double m_sqrt_int(double x)
{
    double y, nextMem = 1.0;

    if(m_equal(x, 0.0)) return 0.0;
    if(m_equal(x, 1.0)) return 1.0;
    
    // Repeat until epsilon is smaller then first decimal digit
    // because of possible rounding of result afterwards.
    do{
        y = nextMem;
        nextMem = (y + x/y) * 0.5;
    }while(m_abs_dbl(nextMem - y) > 0.1);
    
    return y;
}

int32_t mmToInternalStep(double mm)
{
    return (int32_t)(mm / INTERNAL_STEP_MM);
}

int32_t internalToRealStep(int32_t internal, double constant)
{
    return (int32_t)((double)internal * INTERNAL_STEP_MM / constant);
}

/*******************************************************************************
 * Vypis uzivatelske napovedy (funkce se vola pri vykonavani prikazu "help")
 * systemoveho helpu
*******************************************************************************/
void print_user_help(void)
{
}

#define PRINT_BUFFER_SIZE 200
char print_buffer[PRINT_BUFFER_SIZE];
void print_val1(char *info, int32_t v1)
{
    snprintf(print_buffer, PRINT_BUFFER_SIZE, "%s %ld", info, v1);
    term_send_str_crlf(print_buffer);
}

void print_val2(char *info, int32_t v1, int32_t v2)
{
    snprintf(print_buffer, PRINT_BUFFER_SIZE, "%s %ld %ld", info, v1, v2);
    term_send_str_crlf(print_buffer);
}

void drawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void drawCircle (int32_t sx, int32_t sy, int32_t R);

/*******************************************************************************
 * Dekodovani a vykonani uzivatelskych prikazu
*******************************************************************************/
unsigned char decode_user_cmd(char *cmd_ucase, char *cmd)
{
    char *args, *arg, *endptr;
    uint8_t argc;
    int32_t val[4];
    
    if (strcmp4(cmd_ucase, "STOP"))
    {
        currentDrawing = DRAWING_FREE;
        currentComplexDrawing = DRAWING_COMPLEX_FREE;
    }   
    
    if (currentDrawing != DRAWING_FREE || currentComplexDrawing != DRAWING_COMPLEX_FREE)
    {
        term_send_str_crlf("Error: Device have not yet finished operation");
    }
  
    if (strcmp5(cmd_ucase, "LINE ")) 
    {
        // Move to arguments part
        args = cmd + 5;
        arg = strtok(args, " ");
        argc = 0;
        
        while (arg != NULL)
        {
            switch (argc)
            {
            case 0:
                val[0] = mmToInternalStep(strtol(arg, &endptr, 10));
                break;
            case 1:
                val[1] = mmToInternalStep(strtol(arg, &endptr, 10));
                break;
            case 2:
                val[2] = mmToInternalStep(strtol(arg, &endptr, 10));
                break;
            case 3:
                val[3] = mmToInternalStep(strtol(arg, &endptr, 10));
                break;
            default:
                term_send_str_crlf("Too many arguments.");
                return CMD_UNKNOWN;
            }
            argc++;
            
            if ((endptr - arg) != strlen(arg))
            {
                // Argument wasn't fully converted - error
                term_send_str_crlf("Error at argument.");
                return CMD_UNKNOWN;
            }
            
            arg = strtok(NULL, " ");
        }
        
        if (argc != 4)
        {
            term_send_str_crlf("Too few arguments.");
        }
    
    
        term_send_str_crlf("Drawing started.");
        drawLine(val[0], val[1], val[2], val[3]);
    } 
    else if (strcmp7(cmd_ucase, "CIRCLE ")) 
    {
        // Move to arguments part
        args = cmd + 7;
        arg = strtok(args, " ");
        argc = 0;
        
        while (arg != NULL)
        {
            switch (argc)
            {
            case 0:
                val[0] = mmToInternalStep(strtol(arg, &endptr, 10));
                break;
            case 1:
                val[1] = mmToInternalStep(strtol(arg, &endptr, 10));
                break;
            case 2:
                val[2] = mmToInternalStep(strtol(arg, &endptr, 10));
                break;
            default:
                term_send_str_crlf("Too many arguments.");
                return CMD_UNKNOWN;
            }
            argc++;
            
            if ((endptr - arg) != strlen(arg))
            {
                // Argument wasn't fully converted - error
                term_send_str_crlf("Error at argument.");
                return CMD_UNKNOWN;
            }
            
            arg = strtok(NULL, " ");
        }
        
        if (argc != 3)
        {
            term_send_str_crlf("Too few arguments.");
        }
    
        term_send_str_crlf("Drawing started.");
        drawCircle(val[0], val[1], val[2]);
    }
    else if (strcmp4(cmd_ucase, "CUT ")) 
    {
        // Move to arguments part
        args = cmd + 4;
        arg = strtok(args, " ");
        argc = 0;
        
        while (arg != NULL)
        {
            switch (argc)
            {
            case 0:
                val[0] = mmToInternalStep(strtol(arg, &endptr, 10));
                break;
            case 1:
                val[1] = mmToInternalStep(strtol(arg, &endptr, 10));
                break;
            default:
                term_send_str_crlf("Too many arguments.");
                return CMD_UNKNOWN;
            }
            argc++;
            
            if ((endptr - arg) != strlen(arg))
            {
                // Argument wasn't fully converted - error
                term_send_str_crlf("Error at argument.");
                return CMD_UNKNOWN;
            }
            
            arg = strtok(NULL, " ");
        }
        
        if (argc != 2)
        {
            term_send_str_crlf("Too few arguments.");
        }
    
    
        term_send_str_crlf("Drawing started.");
        drawLine(internalHeadX, internalHeadY, val[0], val[1]);
    }
    else if (strcmp4(cmd_ucase, "DEMO"))
    {
        // Set up demo context
        currentDrawing = DRAWING_FREE;
        currentComplexDrawing = DRAWING_COMPLEX_DEMO;
        currentComplexContext.dc.idx = 0;
        term_send_str_crlf("Drawing started.");
    }
    else if (strcmp8(cmd_ucase, "HILBERT "))
    {
        // Move to arguments part
        args = cmd + 8;
        arg = strtok(args, " ");
        argc = 0;
        
        while (arg != NULL)
        {
            switch (argc)
            {
            case 0:
                val[0] = strtol(arg, &endptr, 10);
                break;
            default:
                term_send_str_crlf("Too many arguments.");
                return CMD_UNKNOWN;
            }
            argc++;
            
            if ((endptr - arg) != strlen(arg))
            {
                // Argument wasn't fully converted - error
                term_send_str_crlf("Error at argument.");
                return CMD_UNKNOWN;
            }
            
            arg = strtok(NULL, " ");
        }
        
        if (argc != 1)
        {
            term_send_str_crlf("Too few arguments.");
        }

        // Set up Hilbert context
        currentComplexContext.hc.n = Hilbert_r2n(val[0]);
        // Image size and size of step in millimeters
        double imageSize = 150;
        double stepSize = Hilbert_n2step(currentComplexContext.hc.n, imageSize);
        // Convert step size to number of internal steps
        int32_t internalSteps = mmToInternalStep(stepSize); 
        if (internalSteps > 0)
        {
            currentComplexContext.hc.stepsPerLine = internalSteps;
            currentComplexContext.hc.idx = 0;
            currentComplexContext.hc.length = Hilbert_length(currentComplexContext.hc.n);
            currentDrawing = DRAWING_FREE;
            currentComplexDrawing = DRAWING_COMPLEX_HILBERT;
            term_send_str_crlf("Drawing started.");
            
            // Move to starting position
            currentComplexContext.hc.startX = mmToInternalStep(20);
            currentComplexContext.hc.startY = mmToInternalStep(20);
            drawLine(currentComplexContext.hc.startX, currentComplexContext.hc.startY, 
                     currentComplexContext.hc.startX, currentComplexContext.hc.startY);
        }
        else
        {
            term_send_str_crlf("Can't draw Hilbert's curve, the resolution is too large.");
        }
    }
    else 
    {
        return CMD_UNKNOWN;
    }
    
    return USER_COMMAND;
}


/*******************************************************************************
 * Inicializace periferii/komponent po naprogramovani FPGA
*******************************************************************************/
void fpga_initialized()
{
  term_send_crlf();
  term_send_str_crlf("Aplikacia bezi.");
}

void motorsIdle()
{
    MOTOR_X_PORT = (MOTOR_X_PORT & (~MOTOR_X_PIN_MASK));
    MOTOR_Y_PORT = (MOTOR_Y_PORT & (~MOTOR_Y_PIN_MASK));
    term_send_str_crlf("Entering idle mode.");
}

void initializePen()
{
    PEN_PORT &= (~PEN_MASK);
    penState = PEN_UP;
}

void penUp()
{
    if (penState == PEN_DOWN)
    {
        PEN_PORT &= (~PEN_MASK);
        penState = PEN_UP;
        print_val1("Pen down = ", penState);
        // Wait for pen to rise
        delay_ms(100);
    }
}

void penDown()
{
    if (penState == PEN_UP)
    {
        PEN_PORT |= PEN_MASK;
        penState = PEN_DOWN;
        print_val1("Pen down = ", penState);
        // Wait for pen to touch the paper
        delay_ms(100);
    }
}

void motorStep(uint8_t info)
{
    static uint8_t motorXCurrentPhase = 0;
    static uint8_t motorYCurrentPhase = 0;
    
    static uint8_t lastMotorXDir = MOTOR_BACKWARD;
    static uint8_t lastMotorYDir = MOTOR_BACKWARD;
    
    uint8_t direction = info & MOTOR_DIR_MASK;
    int8_t nextPhase = (direction == MOTOR_FORWARD ? 1 : -1);
    uint8_t nextWord = 0;
        
    if ((info & MOTOR_MASK) == MOTOR_X)
    {
        // Resolve head position
        if ((TOGGLE_X_PORT & TOGGLE_X_MASK) == 0)
        {
            if (headXArea == IN_DRAWING_AREA)
            {
                // Toggle was pressed in previous step.
                if (lastMotorXDir == MOTOR_BACKWARD)
                    headXArea = BEFORE_DRAWING_AREA;
                else
                    headXArea = AFTER_DRAWING_AREA;
            }
        }
        else
        {
            // Head is in drawing area
            headXArea = IN_DRAWING_AREA;
        }
    
        // Prevent moving further outside drawing area
        if ((headXArea == BEFORE_DRAWING_AREA && direction == MOTOR_BACKWARD) || (headXArea == AFTER_DRAWING_AREA && direction == MOTOR_FORWARD))
        {
            set_led_d5(1);
            return;
        }
        
        motorXCurrentPhase = (motorXCurrentPhase + (uint8_t)(nextPhase + motorPhasesCount)) % motorPhasesCount;
        nextWord = motorPhases[motorXCurrentPhase] << MOTOR_X_PIN_OFFSET;
        lastMotorXDir = info & MOTOR_DIR_MASK;
        // Send next word on port while preserving the unused pins
        MOTOR_X_PORT = (MOTOR_X_PORT & (~MOTOR_X_PIN_MASK)) | nextWord;
        //TODO: Debug mode?
        //term_send_str_crlf("Krok X.");
        set_led_d5(0);
    }
    else 
    {
        // Resolve head position
        if ((TOGGLE_Y_PORT & TOGGLE_Y_MASK) == 0)
        {
            if (headYArea == IN_DRAWING_AREA)
            {
                // Toggle was pressed in previous step.
                if (lastMotorYDir == MOTOR_BACKWARD)
                    headYArea = BEFORE_DRAWING_AREA;
                else
                    headYArea = AFTER_DRAWING_AREA;
            }
        }
        else
        {
            // Head is in drawing area
            headYArea = IN_DRAWING_AREA;
        }
    
        // Prevent moving further outside drawing area
        if ((headYArea == BEFORE_DRAWING_AREA && direction == MOTOR_BACKWARD) || (headYArea == AFTER_DRAWING_AREA && direction == MOTOR_FORWARD))
        {
            set_led_d6(1);
            return;
        }
        
        motorYCurrentPhase = (motorYCurrentPhase + (uint8_t)(nextPhase + motorPhasesCount)) % motorPhasesCount;
        nextWord = motorPhases[motorYCurrentPhase] << MOTOR_Y_PIN_OFFSET;
        lastMotorYDir = info & MOTOR_DIR_MASK;
        // Send next word on port while preserving the unused pins
        MOTOR_Y_PORT = (MOTOR_Y_PORT & (~MOTOR_Y_PIN_MASK)) | nextWord;
        //TODO: Debug mode?
        //term_send_str_crlf("Krok Y.");
        set_led_d6(0);
    }
}

void moveToOrigin()
{
    // First move head before drawing area to find origin
    while (headXArea != BEFORE_DRAWING_AREA)
    {
        motorStep(MOTOR_X | MOTOR_BACKWARD);
        delay_ms(DELAY);
    }
    while (headYArea != BEFORE_DRAWING_AREA)
    {
        motorStep(MOTOR_Y | MOTOR_BACKWARD);
        delay_ms(DELAY);
    }
    
    // Then return it to drawing area
    while (headXArea != IN_DRAWING_AREA)
    {
        motorStep(MOTOR_X | MOTOR_FORWARD);
        delay_ms(DELAY);
    }
    while (headYArea != IN_DRAWING_AREA)
    {
        motorStep(MOTOR_Y | MOTOR_FORWARD);
        delay_ms(DELAY);
    }
}

// Return false if at final position, true otherwise
uint8_t moveToward(int32_t x, int32_t y, uint8_t cutting)
{
    if (x > internalHeadX)
    {
        internalHeadX++;
    }
    else if (x < internalHeadX)
    {
        internalHeadX--;
    }
    
    if (y > internalHeadY)
    {
        internalHeadY++;
    }
    else if (y < internalHeadY)
    {
        internalHeadY--;
    }
    
    int32_t newRealX = internalToRealStep(internalHeadX, MOTOR_X_STEP_MM);
    int32_t newRealY = internalToRealStep(internalHeadY, MOTOR_Y_STEP_MM);
    
    if (cutting && headXArea == IN_DRAWING_AREA && headYArea == IN_DRAWING_AREA)
        penDown();
    else
        penUp();
    
    if (newRealX > realHeadX)
    {
        if (headXArea != AFTER_DRAWING_AREA)
        {
            motorStep(MOTOR_X | MOTOR_FORWARD);
            realHeadX++;
        }
    }
    else if (newRealX < realHeadX)
    {
        if (headXArea != BEFORE_DRAWING_AREA)
        {
            motorStep(MOTOR_X | MOTOR_BACKWARD);
            realHeadX--;
        }
    }
    
    if (newRealY > realHeadY)
    {
        if (headYArea != AFTER_DRAWING_AREA)
        {
            motorStep(MOTOR_Y | MOTOR_FORWARD);
            realHeadY++;
        }
    }
    else if (newRealY < realHeadY)
    {
        if (headYArea != BEFORE_DRAWING_AREA)
        {
            motorStep(MOTOR_Y | MOTOR_BACKWARD);
            realHeadY--;
        }
    }
    
    //TODO: Debug mode?
    //print_val2("Head moved to: ", internalHeadX, internalHeadY);
    
    return x == internalHeadX && y == internalHeadY ? OPERATION_FINISHED : OPERATION_IN_PROGRESS;
}

void drawLine (int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    LineContext lc;
    int32_t dx, dy;
    
    // If head already in starting position, begin cutting.
    // Needs to be done before coordinates swap.
    if (x1 == internalHeadX && y1 == internalHeadY)
    {
        lc.state = STATE_CUTTING;
    }
    else
    {
        lc.state = STATE_MOVING;
    }
    
    lc.leftRight = x2 >= x1;

    //Bressenhamov algoritmus
    dx = lc.leftRight ? (x2 - x1) : (x1 - x2);
    dy = m_abs_int(y2 - y1);
    lc.makeSwap = 0;
    if (dx < dy)
    {
        swap(&x1, &y1);
        swap(&x2, &y2);

        lc.leftRight = x2 >= x1;

        dx = lc.leftRight ? (x2 - x1) : (x1 - x2);
        dy = m_abs_int(y2 - y1);
        lc.makeSwap = 1;
    }

    // Fill context with necessary data
    lc.x1 = x1; lc.y1 = y1;
    lc.x2 = x2; lc.y2 = y2;
    lc.dx = dx; lc.dy = dy;
    lc.P1 = 2*dy;
    lc.P2 = lc.P1 - 2*dx;
    lc.P  = 2*dy - dx;
    lc.x = x1; lc.y  = y1;
    lc.ystep = y2 >= y1 ? 1 : -1;
    
    // Prepare global variables
    currentDrawing = DRAWING_LINE;
    currentContext.lc = lc;
    term_send_str_crlf("Moving into starting position.");
}

// Return false if finished, true otherwise
uint8_t drawLineStep(LineContext* lc)
{
    uint8_t tmp;

    switch(lc->state)
    {
    case STATE_MOVING:
        if (lc->makeSwap)
            tmp = moveToward(lc->y1, lc->x1, 0);
        else
            tmp = moveToward(lc->x1, lc->y1, 0);
            
        if (tmp == OPERATION_FINISHED)
        {
            // If finished moving, start cutting
            term_send_str_crlf("Cutting.");
            lc->state = STATE_CUTTING;
        }
        
        return OPERATION_IN_PROGRESS;
    
    case STATE_CUTTING:
        // Is the head in final position?
        if (lc->x == lc->x2)
        {
            lc->state = STATE_FINISHED;
            return OPERATION_FINISHED;
        }
        
        // Perform next step of algorithm
        if(lc->P >= 0)
        {
            lc->P += lc->P2;
            lc->y += lc->ystep;
        }
        else
        {
            lc->P += lc->P1;
        }
        
        if (lc->leftRight)
        {
            lc->x++;
        }
        else
        {
            lc->x--;
        }
        
        // Set new head position
        if (lc->makeSwap)
            moveToward(lc->y, lc->x, 1);
        else
            moveToward(lc->x, lc->y, 1);
        
        return OPERATION_IN_PROGRESS;
        
    default:
        return OPERATION_FINISHED;
    }
}

void drawCircle (int32_t sx, int32_t sy, int32_t R)
{
    // Fill context with necessary data
    CircleContext cc;
    cc.x = 0;
    cc.i = 1; cc.j = 1;
    cc.sx = sx; cc.sy = sy;
    cc.R = R;
    cc.state = STATE_MOVING;
    cc.xGrow = 1;
    
    // Prepare global variables
    currentDrawing = DRAWING_CIRCLE;
    currentContext.cc = cc;
    term_send_str_crlf("Moving into starting position.");

    return;
}

uint8_t drawCircleStep(CircleContext* cc)
{
    int32_t x, y, yTmp;

    switch(cc->state)
    {
    case STATE_MOVING:
        if (moveToward(cc->sx, cc->sy + cc->R, 0) == OPERATION_FINISHED)
        {
            // If finished moving, start cutting
            term_send_str_crlf("Cutting.");
            cc->state = STATE_CUTTING;
        }
        
        return OPERATION_IN_PROGRESS;
    
    case STATE_CUTTING:
        // Perform next step of algorithm
        if (cc->xGrow)
        {
            cc->x++;
            yTmp = m_round(m_sqrt_int((double)(cc->R*cc->R - cc->x*cc->x)));
            if (cc->i * cc->j > 0)
            {
                x = cc->x;
                y = yTmp;
            }
            else
            {
                x = yTmp;
                y = cc->x;
            }
            
            if (cc->x >= yTmp)
            {
                cc->xGrow = 0;
                if (cc->x > yTmp)
                {
                    cc->x--;
                }
            }
        }
        else
        {
            cc->x--;
            yTmp = m_round(m_sqrt_int((double)(cc->R*cc->R - cc->x*cc->x)));
            if (cc->i * cc->j > 0)
            {
                x = yTmp;
                y = cc->x;
            }
            else
            {
                x = cc->x;
                y = yTmp;
            }
            
            if (cc->x == 0)
            {
                cc->xGrow = 1;
            }
        }

        // Set new head position
        moveToward(cc->sx + x * cc->i, cc->sy + y * cc->j, 1);
        
        // Each time x reaches zero, new quadrate begins
        if (cc->x == 0)
        {
            if (cc->i < 0)
            {
                if (cc->j > 0)
                {
                    // i = -1, j = 1 and x = 0 is final position
                    cc->state = STATE_FINISHED;
                    return OPERATION_FINISHED;
                }
                else
                {
                    cc->j = 1;
                }
            }
            else
            {
                if (cc->j > 0)
                {
                    cc->j = -1;
                }
                else
                {
                    cc->i = -1;
                }
            }
        }
        
        return OPERATION_IN_PROGRESS;
        
    default:
        return OPERATION_FINISHED;
    }
}

uint8_t drawDemo(DemoContext* dc)
{
    int32_t idx = dc->idx;
    int32_t item;
    int32_t a, b, c, d;
    
    item = demo[idx++];
    switch (item)
    {
    case DEMO_LINE:
        a = demo[idx++];
        b = demo[idx++];
        c = demo[idx++];
        d = demo[idx++];
        drawLine(a, b, c, d);
        break;
    case DEMO_CIRCLE:
        a = demo[idx++];
        b = demo[idx++];
        c = demo[idx++];
        drawCircle(a, b, c);
        break;
    case DEMO_CUT:
        a = demo[idx++];
        b = demo[idx++];
        drawLine(internalHeadX, internalHeadY, a, b);
        break;
    case DEMO_END:
    default:
        return 1;
    }
    
    dc->idx = idx;
    return 0;
}

uint8_t drawHilbert(HilbertContext* hc)
{
    int32_t hx = 0, hy = 0;
    if (hc->idx < hc->length)
    {
        Hilbert_d2xy(hc->n, hc->idx, &hx, &hy);
        hx = hc->startX + hx * hc->stepsPerLine;
        hy = hc->startY + hy * hc->stepsPerLine;
        drawLine(internalHeadX, internalHeadY, hx, hy);
        hc->idx++;
        return 0;
    }
    
    return 1;
}

/*******************************************************************************
 * Hlavni funkce
*******************************************************************************/
int main()
{
    initialize_hardware();
    WDG_stop();                               // zastav watchdog

    set_led_d5(0);
    set_led_d6(0);

    // Disable modules on ports
    P4SEL = 0x0;
    P6SEL = 0x0;

    // Set up motor ports for output
    MOTOR_X_PORT_DIR |= MOTOR_X_PIN_MASK;
    MOTOR_Y_PORT_DIR |= MOTOR_Y_PIN_MASK;
    // Set up toggle ports for input
    TOGGLE_X_PORT_DIR &= (~TOGGLE_X_MASK);
    TOGGLE_Y_PORT_DIR &= (~TOGGLE_Y_MASK);
    // Set up pen port for output
    PEN_PORT_DIR |= PEN_MASK;

    uint8_t idle = 0;
    uint32_t counter = 0;

    // Wait for ports to set up
    delay_ms(1000);
    initializePen();
    moveToOrigin();

    while (1) {
        switch (currentDrawing)
        {
        case DRAWING_FREE:
            switch (currentComplexDrawing)
            {
            case DRAWING_COMPLEX_DEMO:
                term_send_str_crlf("Drawing demo.");
                if(drawDemo(&(currentComplexContext.dc)))
                {
                    currentDrawing = DRAWING_FREE;
                    currentComplexDrawing = DRAWING_COMPLEX_FREE;
                }
                break;
            case DRAWING_COMPLEX_HILBERT:
                term_send_str_crlf("Drawing Hilbert.");
                if(drawHilbert(&(currentComplexContext.hc)))
                {
                    currentDrawing = DRAWING_FREE;
                    currentComplexDrawing = DRAWING_COMPLEX_FREE;
                }
                break;
            case DRAWING_COMPLEX_FREE:
                if (counter < IDLE_TIME / DELAY)
                {           
                    counter++;
                }
                else if (idle == 0)
                {
                    motorsIdle();
                    penUp();
                    idle = 1;
                }
                break;
            }
            break;
        case DRAWING_LINE:
            if(drawLineStep(&(currentContext.lc)) == OPERATION_FINISHED)
            {
                currentDrawing = DRAWING_FREE;
                idle = 0;
                counter = 0;
                term_send_str_crlf("Drawing finished.");
            }
            break;
        case DRAWING_CIRCLE:
            if(drawCircleStep(&(currentContext.cc)) == OPERATION_FINISHED)
            {
                currentDrawing = DRAWING_FREE;
                idle = 0;
                counter = 0;
                term_send_str_crlf("Drawing finished.");
            }
            break;
        }       
        
        terminal_idle();
        if (headXArea == IN_DRAWING_AREA || headYArea == IN_DRAWING_AREA)
            delay_ms(DELAY);
    }
    
    return 0;
}

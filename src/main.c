#include <stdio.h>
#include <stdlib.h>
#include <esp32/rom/ets_sys.h>
#include <esp_task_wdt.h>
#include "driver/gpio.h"
#include "esp_timer.h"

#define LED_PIN_LEVEL_UP 12
#define LED_PIN_LEVEL_MIDDLE 14
#define LED_PIN_LEVEL_DOWN 27
#define BUTTON_PIN 26

#define PUSH_TIME_US 250000 // 250 ms debounce
#define QUEUE_SIZE 10       // Size of travel needs queue

/* Position definitions */
#define POSITION_LOWER_LEVEL 0
#define POSITION_MIDDLE_LEVEL 1
#define POSITION_UPPER_LEVEL 2

/* Events for state machine */
#define EVENT_BUTTON_PUSH_LOWER 0
#define EVENT_BUTTON_PUSH_MIDDLE 1
#define EVENT_BUTTON_PUSH_UPPER 2
#define EVENT_DONE_LOADING 3
#define EVENT_DONE_UNLOADING 4
#define EVENT_ARRIVE 5
#define EVENT_TIMER_TICK 6

/* State definitions */
#define STATE_IDLE 0
#define STATE_MOVING 1
#define STATE_LOADING 2
#define STATE_UNLOADING 3

// Global variables
int currentPosition;
int movingDirection;
int targetPosition = -1; // -1 means no active destination
uint64_t stateStartTime = 0;
int currentState = STATE_IDLE;

void idle(int event);
void moving(int event);
void loading(int event);
void unloading(int event);

// Initialize with idle state
void (*stateFuncPtr)(int) = idle;

// Represents a passenger travel need
struct travel_need
{
    int origin;
    int destination;
};

// Debounce timing for button presses
static volatile uint64_t lastPush = 0;

// Counter for which travel need to process next
static volatile int travel_need_counter = 0;

// Array holding all predefined travel needs
static volatile struct travel_need travel_needs[50];
struct travel_need active_need; // Current travel being processed by FSM

// Queue for travel needs
static volatile struct travel_need travel_queue[QUEUE_SIZE];
static volatile int queue_front = 0;
static volatile int queue_rear = 0;
static volatile int queue_count = 0;

// Global flag for button press
static volatile bool button_pressed = false;

// Update LED display based on current position
void updateLEDs()
{
    gpio_set_level(LED_PIN_LEVEL_UP, (currentPosition == POSITION_UPPER_LEVEL) ? 1 : 0);
    gpio_set_level(LED_PIN_LEVEL_MIDDLE, (currentPosition == POSITION_MIDDLE_LEVEL) ? 1 : 0);
    gpio_set_level(LED_PIN_LEVEL_DOWN, (currentPosition == POSITION_LOWER_LEVEL) ? 1 : 0);
}

// Queue management functions
int enqueue_travel_need(struct travel_need need) {
    if (queue_count >= QUEUE_SIZE) {
        printf("[QUEUE] Full - cannot add request\n");
        return -1;
    }
    
    travel_queue[queue_rear] = need;
    queue_rear = (queue_rear + 1) % QUEUE_SIZE;
    queue_count++;
    return 0;
}

int dequeue_travel_need(struct travel_need *need) {
    if (queue_count <= 0) {
        return -1; // Queue empty
    }
    
    *need = travel_queue[queue_front];
    queue_front = (queue_front + 1) % QUEUE_SIZE;
    queue_count--;
    return 0;
}

int is_queue_empty() {
    return queue_count == 0;
}

// Process next travel need from queue
void process_next_from_queue() {
    struct travel_need next_need;
    if (dequeue_travel_need(&next_need) == 0) {
        printf("\n[QUEUE] Processing: Floor %d -> Floor %d\n",
               next_need.origin, next_need.destination);
        
        active_need = next_need;
        targetPosition = active_need.origin;
        
        // Check if already at pickup floor
        if (currentPosition == targetPosition) {
            printf("[ELEVATOR] Already at pickup floor %d\n", targetPosition);
            stateStartTime = esp_timer_get_time();
            stateFuncPtr = loading;
            currentState = STATE_LOADING;
        } else {
            movingDirection = (targetPosition > currentPosition) ? 1 : -1;
            printf("[ELEVATOR] Moving %s: Floor %d -> Floor %d\n", 
                   (movingDirection == 1) ? "UP" : "DOWN", 
                   currentPosition, targetPosition);
            stateStartTime = esp_timer_get_time();
            stateFuncPtr = moving;
            currentState = STATE_MOVING;
        }
    }
}

// Simple interrupt handler - only sets flag
static void IRAM_ATTR handle_push(void *arg)
{
    uint64_t now = esp_timer_get_time();
    if ((now - lastPush) > PUSH_TIME_US) {
        button_pressed = true;
        lastPush = now;
    }
}

void app_main()
{
    // Initialize 50 randomly generated travel needs
    travel_needs[0].origin = 2; travel_needs[0].destination = 1;
    travel_needs[1].origin = 1; travel_needs[1].destination = 2;
    travel_needs[2].origin = 1; travel_needs[2].destination = 2;
    travel_needs[3].origin = 0; travel_needs[3].destination = 2;
    travel_needs[4].origin = 2; travel_needs[4].destination = 1;
    travel_needs[5].origin = 0; travel_needs[5].destination = 2;
    travel_needs[6].origin = 1; travel_needs[6].destination = 2;
    travel_needs[7].origin = 1; travel_needs[7].destination = 0;
    travel_needs[8].origin = 0; travel_needs[8].destination = 1;
    travel_needs[9].origin = 1; travel_needs[9].destination = 0;
    travel_needs[10].origin = 1; travel_needs[10].destination = 2;
    travel_needs[11].origin = 0; travel_needs[11].destination = 1;
    travel_needs[12].origin = 0; travel_needs[12].destination = 2;
    travel_needs[13].origin = 0; travel_needs[13].destination = 1;
    travel_needs[14].origin = 0; travel_needs[14].destination = 2;
    travel_needs[15].origin = 0; travel_needs[15].destination = 1;
    travel_needs[16].origin = 2; travel_needs[16].destination = 1;
    travel_needs[17].origin = 2; travel_needs[17].destination = 1;
    travel_needs[18].origin = 1; travel_needs[18].destination = 0;
    travel_needs[19].origin = 2; travel_needs[19].destination = 1;
    travel_needs[20].origin = 1; travel_needs[20].destination = 0;
    travel_needs[21].origin = 0; travel_needs[21].destination = 1;
    travel_needs[22].origin = 1; travel_needs[22].destination = 2;
    travel_needs[23].origin = 0; travel_needs[23].destination = 2;
    travel_needs[24].origin = 2; travel_needs[24].destination = 1;
    travel_needs[25].origin = 1; travel_needs[25].destination = 0;
    travel_needs[26].origin = 1; travel_needs[26].destination = 2;
    travel_needs[27].origin = 0; travel_needs[27].destination = 2;
    travel_needs[28].origin = 1; travel_needs[28].destination = 0;
    travel_needs[29].origin = 1; travel_needs[29].destination = 2;
    travel_needs[30].origin = 0; travel_needs[30].destination = 1;
    travel_needs[31].origin = 1; travel_needs[31].destination = 2;
    travel_needs[32].origin = 0; travel_needs[32].destination = 2;
    travel_needs[33].origin = 0; travel_needs[33].destination = 2;
    travel_needs[34].origin = 1; travel_needs[34].destination = 2;
    travel_needs[35].origin = 2; travel_needs[35].destination = 1;
    travel_needs[36].origin = 0; travel_needs[36].destination = 2;
    travel_needs[37].origin = 1; travel_needs[37].destination = 0;
    travel_needs[38].origin = 0; travel_needs[38].destination = 2;
    travel_needs[39].origin = 2; travel_needs[39].destination = 1;
    travel_needs[40].origin = 0; travel_needs[40].destination = 1;
    travel_needs[41].origin = 0; travel_needs[41].destination = 1;
    travel_needs[42].origin = 0; travel_needs[42].destination = 1;
    travel_needs[43].origin = 1; travel_needs[43].destination = 0;
    travel_needs[44].origin = 0; travel_needs[44].destination = 2;
    travel_needs[45].origin = 2; travel_needs[45].destination = 1;
    travel_needs[46].origin = 2; travel_needs[46].destination = 1;
    travel_needs[47].origin = 2; travel_needs[47].destination = 1;
    travel_needs[48].origin = 0; travel_needs[48].destination = 2;
    travel_needs[49].origin = 1; travel_needs[49].destination = 0;

    gpio_config_t config;

    // Configure LEDs as output
    config.mode = GPIO_MODE_OUTPUT;
    config.pin_bit_mask = (1ULL << LED_PIN_LEVEL_UP) | (1ULL << LED_PIN_LEVEL_MIDDLE) | (1ULL << LED_PIN_LEVEL_DOWN);
    config.pull_up_en = 0;
    config.pull_down_en = 0;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);

    // Configure button as input with interrupt
    config.intr_type = GPIO_INTR_NEGEDGE;
    config.mode = GPIO_MODE_INPUT;
    config.pin_bit_mask = (1ULL << BUTTON_PIN);
    config.pull_up_en = 1;
    config.pull_down_en = 0;
    gpio_config(&config);

    // Activate interrupts for GPIOs
    esp_err_t res;
    res = gpio_install_isr_service(0);
    ESP_ERROR_CHECK(res);

    // Add handler to ISR for button pin
    res = gpio_isr_handler_add(BUTTON_PIN, handle_push, NULL);
    ESP_ERROR_CHECK(res);

    // Initialize system
    currentPosition = POSITION_LOWER_LEVEL;
    updateLEDs();

    printf("\n========== ELEVATOR SYSTEM STARTED ==========\n");
    printf("[SYSTEM] Initial position: Floor %d (LOWER)\n", currentPosition);
    printf("[SYSTEM] State: IDLE - Ready for requests\n");
    printf("[INFO] Press button to add travel requests\n");
    printf("=============================================\n\n");

    uint64_t lastTick = esp_timer_get_time();

    // Main loop with timing
    while (1)
    {
        uint64_t now = esp_timer_get_time();

        // Handle button press in safe context
        if (button_pressed) {
            button_pressed = false;
            
            // Get next travel need from list
            struct travel_need new_need = travel_needs[travel_need_counter];
            
            printf("\n[BUTTON] Request #%d: Floor %d -> Floor %d (Queued: %d)\n",
                   travel_need_counter + 1, new_need.origin, new_need.destination, queue_count);

            // Add to queue
            if (enqueue_travel_need(new_need) == 0) {
                printf("[QUEUE] Added successfully. Queue size: %d\n", queue_count);
            }

            // Increment counter
            travel_need_counter++;
            if (travel_need_counter >= 50) {
                travel_need_counter = 0;
            }
        }

        // Send timer event every second for state machine timing
        if ((now - lastTick) > 1000000) { // 1 second
            lastTick = now;
            if (stateFuncPtr != NULL) {
                stateFuncPtr(EVENT_TIMER_TICK);
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void idle(int event)
{
    static int buttonProcessed = 0;

    if (event == EVENT_TIMER_TICK)
    {
        // Check queue for waiting travel needs
        if (!is_queue_empty() && currentState == STATE_IDLE) {
            process_next_from_queue();
        }
        
        if (!buttonProcessed) {
            buttonProcessed = 1;
        }
        return;
    }

    if (event == EVENT_BUTTON_PUSH_LOWER || event == EVENT_BUTTON_PUSH_MIDDLE || event == EVENT_BUTTON_PUSH_UPPER)
    {
        buttonProcessed = 0;

        // If already at target floor, go directly to loading
        if (currentPosition == targetPosition)
        {
            stateStartTime = esp_timer_get_time();
            stateFuncPtr = loading;
            currentState = STATE_LOADING;
        }
        else
        {
            // Determine direction
            movingDirection = (targetPosition > currentPosition) ? 1 : -1;
            stateStartTime = esp_timer_get_time();
            stateFuncPtr = moving;
            currentState = STATE_MOVING;
        }
    }
}

void moving(int event)
{
    static int movingStarted = 0;

    if (event == EVENT_TIMER_TICK)
    {
        if (!movingStarted)
        {
            // Turn off all LEDs while moving
            gpio_set_level(LED_PIN_LEVEL_UP, 0);
            gpio_set_level(LED_PIN_LEVEL_MIDDLE, 0);
            gpio_set_level(LED_PIN_LEVEL_DOWN, 0);
            movingStarted = 1;
            return;
        }

        uint64_t now = esp_timer_get_time();
        if ((now - stateStartTime) >= 5000000) { // 5 seconds per floor
            // Move elevator one floor
            currentPosition += movingDirection;

            // Check if destination reached
            if (currentPosition == targetPosition)
            {
                printf("[ELEVATOR] Arrived at Floor %d\n", currentPosition);
                updateLEDs();
                movingStarted = 0;
                stateStartTime = esp_timer_get_time();
                stateFuncPtr = loading;
                currentState = STATE_LOADING;
            }
            else
            {
                // Continue moving
                stateStartTime = now;
            }
        }
    }
}

void loading(int event)
{
    static int loadingStarted = 0;

    if (event == EVENT_TIMER_TICK)
    {
        if (!loadingStarted)
        {
            printf("[PASSENGER] Boarding at Floor %d (Destination: Floor %d)\n", 
                   currentPosition, active_need.destination);
            loadingStarted = 1;
            stateStartTime = esp_timer_get_time();
            return;
        }

        uint64_t now = esp_timer_get_time();
        if ((now - stateStartTime) >= 5000000) { // 5 seconds loading time
            loadingStarted = 0;
            targetPosition = active_need.destination;

            // Determine direction to destination
            if (targetPosition > currentPosition) {
                movingDirection = 1;
            }
            else if (targetPosition < currentPosition) {
                movingDirection = -1;
            }

            stateStartTime = esp_timer_get_time();

            if (targetPosition == currentPosition)
            {
                // Same floor, go directly to unloading
                stateFuncPtr = unloading;
                currentState = STATE_UNLOADING;
            }
            else
            {
                // Need to move to destination
                stateFuncPtr = moving;
                currentState = STATE_MOVING;
            }
        }
    }
}

void unloading(int event)
{
    static int unloadingStarted = 0;

    if (event == EVENT_TIMER_TICK)
    {
        if (!unloadingStarted)
        {
            printf("[PASSENGER] Exiting at Floor %d\n", currentPosition);
            unloadingStarted = 1;
            stateStartTime = esp_timer_get_time();
            return;
        }

        uint64_t now = esp_timer_get_time();
        if ((now - stateStartTime) >= 5000000) { // 5 seconds unloading time
            printf("[ELEVATOR] Ready for next passenger\n");
            unloadingStarted = 0;
            targetPosition = -1;

            // Check if more requests in queue
            if (!is_queue_empty()) {
                printf("[QUEUE] %d request(s) waiting\n\n", queue_count);
            } else {
                printf("\n");
            }
            
            stateFuncPtr = idle;
            currentState = STATE_IDLE;
        }
    }
}
#define F_CPU 32000000UL

#define ADC_REF_VOLTAGE 1
#define ADC_BIT_RES 4096

//Main Power rail ADC
#define MP_ON_THRESHOLD_V 11.9
#define MP_FAIL_THRESHOLD_V 11
#define MP_R_SERIES 12
#define MP_R_MEASURE 1
#define MP_ON_ADC_THRES ( ((float)MP_R_MEASURE / (float)(MP_R_SERIES + MP_R_MEASURE) ) * (ADC_BIT_RES / (float)ADC_REF_VOLTAGE) * (float)MP_ON_THRESHOLD_V )
#define MP_FAIL_ADC_THRES ( ((float)MP_R_MEASURE / (float)(MP_R_SERIES + MP_R_MEASURE) ) * (ADC_BIT_RES / (float)ADC_REF_VOLTAGE) * (float)MP_FAIL_THRESHOLD_V )

//FPGA 1.8V Power rail ADC
#define V1_8_OFF_THRESHOLD_V 0.1
#define V1_8_R_SERIES 10
#define V1_8_R_MEASURE 10
#define V1_8_OFF_ADC_THRES ( ((float)V1_8_R_MEASURE / (float)(V1_8_R_SERIES + V1_8_R_MEASURE) ) * (ADC_BIT_RES / (float)ADC_REF_VOLTAGE) * (float)V1_8_OFF_THRESHOLD_V )

//IO 1.8V Rower rail ADC
#define V1_8IO_OFF_THRESHOLD_V 0.1
#define V1_8IO_R_SERIES 10
#define V1_8IO_R_MEASURE 10
#define V1_8IO_OFF_ADC_THRES ( ((float)V1_8IO_R_MEASURE / (float)(V1_8IO_R_SERIES + V1_8IO_R_MEASURE) ) * (ADC_BIT_RES / (float)ADC_REF_VOLTAGE) * (float)V1_8IO_OFF_THRESHOLD_V )

//FPGA Core Power Rail
#define CORE_OFF_THRESHOLD_V 0.1
#define CORE_R_SERIES 2
#define CORE_R_MEASURE 10
#define CORE_OFF_ADC_THRES ( ((float)CORE_R_MEASURE / (float)(CORE_R_SERIES + CORE_R_MEASURE) ) * (ADC_BIT_RES / (float)ADC_REF_VOLTAGE) * (float)CORE_OFF_THRESHOLD_V )

typedef enum {low = 0, high = 1, toggle} io_level_t;

//Output function

void enableCoreVoltage (io_level_t level);

void enable1_8V (io_level_t level);

void enable1_8VIO (io_level_t level);

void enable5V (io_level_t level);

//Input functions

int16_t readPGoodCore (void);

int16_t readPGood1_8V (void);

int16_t readPGood1_8VIO (void);

int16_t readPGood5V (void);

//ADC functions

void ADC_init(void); // Init ADCs

uint16_t read_MP_ADC(void);     // Read Main Power Voltage ADC

uint16_t read_V1_8IO_ADC(void); // Read 1.8V IO Voltage ADC

uint16_t read_V1_8_ADC(void);  // Read FGPA 1.8V Voltage ADC

uint16_t read_CORE_ADC(void);   // Read FGPA Core Voltage ADC

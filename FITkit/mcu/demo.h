#define DEMO_LINE 0
#define DEMO_CIRCLE 1
#define DEMO_CUT 2
#define DEMO_END 3

int32_t demo[] = {DEMO_CIRCLE, 250, 250, 150,
                  DEMO_CIRCLE, 450, 250, 150,
				  DEMO_CIRCLE, 650, 250, 150,
				  DEMO_CIRCLE, 350, 450, 150,
				  DEMO_CIRCLE, 550, 450, 150,
				  DEMO_LINE, 250, 250, 250, 250,
				  DEMO_CUT, 350, 450,
                  DEMO_CUT, 450, 250,
				  DEMO_CUT, 550, 450,
				  DEMO_CUT, 650, 250,
				  DEMO_END};

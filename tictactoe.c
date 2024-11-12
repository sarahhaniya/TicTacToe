
#include <stdbool.h> // Include Standard Boolean Library for boolean variables
#include <stdio.h> // Include Standard Input Output Library for basic I/O operations


// Function prototypes for configuring and handling interrupts
void disableInterrupts(void);// Disables interrupts in the ARM A9 processor
void setIRQStack(void);// Sets up the stack for IRQ mode
void configureGIC(void);// Configures the General Interrupt Controller (GIC)
void configureKEYs(void);// Configures the FPGA's PS/2 port for keygameBoard interrupts
void enable_A9_interrupts(void);// Enables interrupts in the ARM A9 processor
void keygameBoard_ISR(void);// Interrupt Service Routine for keygameBoard interrupts
void configureinterrupt(int, int);// Configures individual interrupts

// Function prototypes for rendering and game logic
void render_player(int gameBoardIndex);
void render_player_X(int gameBoardIndex);
void render_player_O(int gameBoardIndex);
void initial_screen();// Sets up the initial game screen
void draw_pixel(int x, int y, short int line_color); // Draws a single pixel on the screen
void render_line(int x0, int y0, int x1, int y1, short int line_color);// Renders a line on the screen
void render_selection_box(int x, int y, short int selection_colour);// Renders a selection box on the screen
void swap(int *first, int *second);// Swaps two integers
void render_gameBoard(void);// Renders the tic-tac-toe gameBoard
void display_text(int x, int y, char * text_ptr);// Writes text to the screen
void delete_screen(); // Clears the graphical screen




// Functions which handle the tic-tac-toe logic
int check_winner(); // Checks for a winner in the game
void delete_text (); // Clears any text from the screen
void checkforDraw();  // Checks for a Draw condition in the game

// Global variables
int selX; // X position of the selection box
int selY; // Y position of the selection box
int xScore = 0; // Score of player X
int OScore = 0; // Score of player O
int totalMatchesPlayed = 0; // Total number of matches played
char scoreStr[10];
bool isDraw = false; // Flag for Draw condition
char Turn; // Indicates whose turn it is ('X' or 'O')
int gameBoard[9]; 
volatile int framebufferStart; // global variable, to render 

int main(void) {
	delete_text();
	
	// First turn goes to X
	Turn = 'X';
	
	// This is the top left corner of the first box
	// Red selection box starts here initially
	selX = 25;
	selY = 25;
	
	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    
	/* Read location of the pixel buffer from the pixel buffer controller */
    framebufferStart = *pixel_ctrl_ptr;
	
	delete_screen();
	start_screen();
	
	disableInterrupts(); // disable interrupts in the A9 processor
	setIRQStack(); // initialize the stack pointer for IRQ mode
	configureGIC(); // configure the general interrupt controller
	configureKEYs(); // configure pushbutton KEYs to generate interrupts
	enable_A9_interrupts(); // enable interrupts in the A9 processor
	
	while (1); // wait for an interrupt
}


/* setup the PS/2 interrupts in the FPGA */
void configureKEYs() {
	volatile int * PS2_ptr = (int *) 0xFF200100; // PS/2 base address
	*(PS2_ptr + 1) = 0x00000001; // set RE to 1 to enable interrupts
}

// Define the IRQ exception handler
void __attribute__((interrupt)) __cs3_isr_irq(void) {
	// Read the ICCIAR from the CPU Interface in the GIC
	int interrupt_ID = *((int *)0xFFFEC10C);
	if (interrupt_ID == 79) // check if interrupt is from the KEYs
	keygameBoard_ISR();
	else
	while (1); // if unexpected, then stay here
	// Write to the End of Interrupt Register (ICCEOIR)
	*((int *)0xFFFEC110) = interrupt_ID;
}

// Define the remaining exception handlers
void __attribute__((interrupt)) __cs3_reset(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_undef(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_swi(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_pabort(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_dabort(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_fiq(void) {
	while (1);
}

//Initialize the banked stack pointer register for IRQ mode
void setIRQStack(void) {
	int stack, mode;
	stack = 0xFFFFFFFF - 7; // top of A9 onchip memory, aligned to 8 bytes
	/* change processor to IRQ mode with interrupts disabled */
	mode = 0b11010010;
	asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
	/* set banked stack pointer */
	asm("mov sp, %[ps]" : : [ps] "r"(stack));
	/* go back to SVC mode before executing subroutine return! */
	mode = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
}

/*
* Turn on interrupts in the ARM processor
*/
void enable_A9_interrupts(void) {
	int status = 0b01010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}

// Turn off interrupts in the ARM processor
void disableInterrupts(void) {
	int status = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}

/*
* Configure the Generic Interrupt Controller (GIC)
*/
void configureGIC(void) {
	configureinterrupt (79, 1); // configure the FPGA KEYs interrupt (73)
	// Set Interrupt Priority Mask Register (ICCPMR). Enable interrupts of all
	// priorities
	*((int *) 0xFFFEC104) = 0xFFFF;
	// Set CPU Interface Control Register (ICCICR). Enable signaling of
	// interrupts
	*((int *) 0xFFFEC100) = 1;
	// Configure the Distributor Control Register (ICDDCR) to send pending
	// interrupts to CPUs
	*((int *) 0xFFFED000) = 1;
}

/*
* Configure Set Enable Registers (ICDISERn) and Interrupt Processor Target
* Registers (ICDIPTRn). The default (reset) values are used for other registers
* in the GIC.
*/
void configureinterrupt(int N, int CPU_target) {
	int reg_offset, index, value, address;
	/* Configure the Interrupt Set-Enable Registers (ICDISERn).
	* reg_offset = (integer_div(N / 32) * 4
	* value = 1 << (N mod 32) */
	reg_offset = (N >> 3) & 0xFFFFFFFC;
	index = N & 0x1F;
	value = 0x1 << index;
	address = 0xFFFED100 + reg_offset;
	/* Now that we know the register address and value, set the appropriate bit */
	*(int *)address |= value;

	/* Configure the Interrupt Processor Targets Register (ICDIPTRn)
	* reg_offset = integer_div(N / 4) * 4
	* index = N mod 4 */
	reg_offset = (N & 0xFFFFFFFC);
	index = N & 0x3;
	address = 0xFFFED800 + reg_offset + index;
	/* Now that we know the register address and value, write to (only) the
	* appropriate byte */
	*(char *)address = (char)CPU_target;
}

void draw_pixel(int x, int y, short int line_color)
{
    *(short int *)(framebufferStart + (y << 10) + (x << 1)) = line_color;
}

// Clear screen by writing black into the address
void delete_screen (){
	int y,x;
	for(x=0;x<320;x++){
		for(y=0;y<240;y++){
			draw_pixel(x,y,0x0000);
		}
	}
}

// Clear any text on the screen by writing " " into the address
void delete_text (){
	int y,x;
	for(x=0;x<80;x++){
		for(y=0;y<60;y++){
			char clear[1] = " \0";
			display_text(x, y, clear);
		}
	}
}

// Function which handles what to do once a keygameBoard interrupt is given
void keygameBoard_ISR(void) {

	volatile int * PS2_ptr = (int *)0xFF200100; // Points to PS2 Base
	unsigned char byte0 = 0;
    
	int PS2_data = *(PS2_ptr);
	int RVALID = PS2_data & 0x8000;
	
	//Read Interrupt Register
	int readInterruptReg;
	readInterruptReg = *(PS2_ptr + 1 ); 
	  
	//Clear Interrupt 
	*(PS2_ptr+1) = readInterruptReg; 

	// when RVALID is 1, there is data 
	if (RVALID != 0){         
		byte0 = (PS2_data & 0xFF); //data in LSB	
	
		if(byte0 == 0x22){  //X, start game
			delete_screen();
			delete_text();
			render_gameBoard();
			render_selection_box(selX, selY, 0xF800);
			isDraw = false;
			char player_status[150] = "                    Player X's Turn!                      \0";
			display_text(14, 55, player_status);
		}
		
		if(byte0 == 0x1D){  //UP, W
			// Erase currently rendern selection box by rendering it black
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			selY = selY - 63;
			
			// Loop back to the first box
			if (selY == -38){
				selY = 151;
			}
			
			render_selection_box(selX, selY, 0xF800);
		}

		if(byte0 == 0x1B){ //DOWN, S
			// Erase currently rendern selection box by rendering it black
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			selY = selY + 63;
			
			// Loop back to the first box
			if (selY == 214){
				selY = 25;
			}
			
			render_selection_box(selX, selY, 0xF800);
		}
	
		if(byte0 == 0x1C){ //LEFT, A
			// Erase currently rendern selection box by rendering it black
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			selX = selX - 90;
			
			// Loop back to the first box
			if (selX == -65){
				selX = 205;
			}
			
			render_selection_box(selX, selY, 0xF800);
		}

		if(byte0 == 0x23){ //RIGHT, D
			// Erase currently rendern selection box by rendering it black
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			selX = selX + 90;
			
			// Loop back to the first box
			if (selX == 295){
				selX = 25;
			}
			
			render_selection_box(selX, selY, 0xF800);
		}

		if(byte0 == 0x29){  //SpaceBar , Restart Game
			delete_screen(0,0,0x0000); 
			delete_text();
			render_gameBoard();
			
			Turn = 'X';
			memset(gameBoard, 0, sizeof(gameBoard));
			
			char delete_winner_status[150] = "                                                     \0";                             
			display_text(14, 55, delete_winner_status);
			
			// Reinitialize selection box to the top left box
			selX = 25;
			selY = 25;
			render_selection_box(selX, selY, 0xF800);
			
			char player_status[150] = "                    Player X's Turn!                      \0";
			display_text(14, 55, player_status);
			isDraw = false;

		}  
		if(byte0 == 0x32){  //B , SCOREBOARD
			delete_screen(0,0,0x0000); 
			delete_text();

			display_text(34, 20, "SCORE BOARD");
			display_text(36, 30, "X:      ");

    		sprintf(scoreStr, "%d", xScore); // Convert xScore to string
    		display_text(39, 30, scoreStr); // Display xScore

			display_text(36, 34, "O:      ");
    		sprintf(scoreStr, "%d", OScore); // Convert OScore to string
    		display_text(39, 34, scoreStr); // Display OScore

			display_text(17, 38, "TOTAL MATCHES PLAYED:");
			sprintf(scoreStr, "%d", totalMatchesPlayed); // Convert OScore to string
    		display_text(39, 38, scoreStr); // Display OScore

			display_text(25, 54, "PRESS [ESC] TO GO BACK TO GAME");
		}
		
		if(byte0 == 0x16){ //Select Box 1 
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			
			selX = 25;
			selY = 25;
			
			render_selection_box(selX, selY, 0xF800);
		}
		
		if(byte0 == 0x1E){ //Select Box 2 
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			
			selX = 115;
			selY = 25;
			
			render_selection_box(selX, selY, 0xF800);

		}
		
		if(byte0 == 0x26){ //Select Box 3 
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			
			selX = 205;
			selY = 25;
			
			render_selection_box(selX, selY, 0xF800);
		}
		
		if(byte0 == 0x25){//Select Box 4
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			
			selX = 25;
			selY = 88;
			
			render_selection_box(selX, selY, 0xF800);
		}
		
		if(byte0 == 0x2E){//Select Box 5
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			
			selX = 115;
			selY = 88;
			
			render_selection_box(selX, selY, 0xF800);
		}
		
		if(byte0 == 0x36){//Select Box 6
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			
			selX = 205;
			selY = 88;
			
			render_selection_box(selX, selY, 0xF800);
		}
		
		if(byte0 == 0x3D){//Select Box 7
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			
			selX = 25;
			selY = 151;
			
			render_selection_box(selX, selY, 0xF800);
		}
		
		if(byte0 == 0x3E){//Select Box 8
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			
			selX = 115;
			selY = 151;
			
			render_selection_box(selX, selY, 0xF800);
		}
		
		if(byte0 == 0x46){//Select Box 9
			render_selection_box(selX, selY, 0x0000);
			render_gameBoard();
			
			selX = 205;
			selY = 151;
			
			render_selection_box(selX, selY, 0xF800);
		}
		
		if(byte0 == 0x33){//H-Help Screen
			delete_screen();			
			delete_text();
			
			char title[100] = "Tic-Tac-Toe Help Screen\0";
			display_text(28, 3, title);

			char instructions[100] = "Try to get 3 consecutive boxes to win the game!\0";
			display_text(8, 7, instructions);
			
			char controls[20] = "Game Controls: \0";
			display_text(8, 10, controls);
			
			char number_keys[70] = "[1]-[9]: Select gameBoard index\0";
			display_text(8, 13, number_keys);
			
			char selection_key_a[70] = "[A]: Move red selection box left\0";
			display_text(8, 15, selection_key_a);
			
			char selection_key_d[70] = "[D]: Move red selection box right\0";
			display_text(8, 17, selection_key_d);
			
			char selection_key_w[70] = "[W]: Move red selection box up\0";
			display_text(8, 19, selection_key_w);
			
			char selection_key_s[70] = "[S]: Move red selection box down\0";
			display_text(8, 21, selection_key_s);	
			
			char enter[70] = "[enter]: Place piece/ Make a move\0";
			display_text(8, 23, enter);
			
			char help[70] = "[H]: Help screen\0";
			display_text(8, 25, help);

			char spacebar[70] = "[spacebar]: Restart game\0";
			display_text(8, 29, spacebar);	

			char scoreBoard[100] = "[S] Score Board\0";
			display_text(8, 3, title);
			
			
			char resume[70] = "Press [ESC] to resume the game\0";
			display_text(8, 31, resume);	
		}
		
		if(byte0 == 0x76){ //Escape - Resume game
			delete_screen();
			delete_text();
			render_gameBoard();
			render_selection_box(selX, selY, 0xF800);
			volatile int i;
			for (i = 0; i < 9; i++){
				if (gameBoard[i] == 1){
					render_player_X(i+1);
				} else if (gameBoard[i] == 2){
					render_player_O(i+1);
				}
			}
			
			if (Turn == 'X'){
				Turn = 'O';
				char player_status[150] = "                    Player O's Turn!                      \0";
				display_text(14, 55, player_status);
			} else {
				Turn = 'X';
				char player_status[150] = "                    Player X's Turn!                      \0";
				display_text(14, 55, player_status);
			}
		}
		
		if(byte0 == 0x5A){ //Enter - place piece on gameBoard
			// check which gameBoard index 
			int gameBoardIndex = 0; 
			
			if (selX == 25 & selY == 25){
				gameBoardIndex = 1; 
			} else if (selX == 115 & selY == 25){
				gameBoardIndex = 2; 
			} else if (selX == 205 & selY == 25){
				gameBoardIndex = 3; 
			} else if (selX == 25 & selY == 88){
				gameBoardIndex = 4; 
			} else if (selX == 115 & selY == 88){
				gameBoardIndex = 5; 
			} else if (selX == 205 & selY == 88){
				gameBoardIndex = 6; 
			} else if (selX == 25 & selY == 151){
				gameBoardIndex = 7; 
			} else if (selX == 115 & selY == 151){
				gameBoardIndex = 8; 
			} else if (selX == 205 & selY == 151){
				gameBoardIndex = 9; 
			}
			
			// Only render if box is empty
			if (gameBoard[gameBoardIndex - 1] == 0){
				
				// update array	
				// 0 means empty, 1 means there is an X, 2 means there is an O
				if (Turn == 'X'){
					gameBoard[gameBoardIndex - 1] = 1;
				} else {
					gameBoard[gameBoardIndex - 1] = 2;
				}
				
				// render player
				render_player(gameBoardIndex);
				
				// check winner
				int winner = check_winner();
				
				// No winner
				if (winner == 0){
					// Switch turn 
					if (Turn == 'X'){
						Turn = 'O';
						char player_status[150] = "                    Player O's Turn!                      \0";
						display_text(14, 55, player_status);
					} else {
						Turn = 'X';
						char player_status[150] = "                    Player X's Turn!                      \0";
						display_text(14, 55, player_status);
					}
					
				// X wins
				} else if (winner == 1){
					// hide selection box
					render_selection_box(selX, selY, 0x0000);
					render_gameBoard();
					
					// show winner status & prompt new game
					char winner_status[150] = "Player X Wins! Press [spacebar] to start a new game.\0";
					display_text(14, 55, winner_status);
					xScore++;
					totalMatchesPlayed++;
					
					
				// O wins
				} else if (winner == 2){
					// hide selection box
					render_selection_box(selX, selY, 0x0000);
					render_gameBoard();
					
					// show winner status & prompt new game
					char winner_status[150] = "Player O Wins! Press [spacebar] to start a new game.\0";
					display_text(14, 55, winner_status);
					OScore++;
					totalMatchesPlayed++;
				// Draw
				} else if (winner == 3){
					// hide selection box
					render_selection_box(selX, selY, 0x0000);
					render_gameBoard();
					
					// show tie status & prompt new game
					char winner_status[150] = "It's a tie! Press [spacebar] to start a new game.\0";
					display_text(14, 55, winner_status);
					totalMatchesPlayed++;
				}
			}
		}
				
		if(byte0 == 0xF0) {
			// Check for break
			switch (PS2_data & 0xFF) {
				case 0x1D:
					break;
				case 0x1B:
					break;
				case 0x1C:
					break;
				case 0x23:
					break;
				default:
					break;
			}	
		}
					
	} 
	return;
}

void render_line(int x0, int y0, int x1, int y1, short int line_color) {
    bool is_steep = ( abs(y1 - y0) > abs(x1 - x0) );
	
    if (is_steep) {
        swap(&x0, &y0);
        swap(&x1, &y1);
    }
   
    if (x0 > x1) {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }
    
    int delta_x = x1 - x0;
    int delta_y = abs(y1 - y0);
    int error = -(delta_x / 2);
    
    int y = y0;
    int y_step;
    if (y0 < y1) 
        y_step =1;
    else 
        y_step = -1;
    volatile int x;
    for(x = x0; x <= x1; x++) {
        if (is_steep) 
            draw_pixel(y, x, line_color);
        else 
            draw_pixel(x, y, line_color);
        
        error += delta_y;
        
        if (error >= 0) {
            y +=y_step;
            error -= delta_x;
        }
    } 
}

void swap(int *first, int *second){
	int temp = *first;
    *first = *second;
    *second = temp;   
}

void render_gameBoard(void){
	render_line(114, 25, 114, 213, 0XFFFF);
	render_line(115, 25, 115, 213, 0XFFFF);
	render_line(116, 25, 116, 213, 0XFFFF);
	
	render_line(204, 25, 204, 213, 0XFFFF);
	render_line(205, 25, 205, 213, 0XFFFF);
	render_line(206, 25, 206, 213, 0XFFFF);

	render_line(25, 87, 295, 87, 0XFFFF);
	render_line(25, 88, 295, 88, 0XFFFF);
	render_line(25, 89, 295, 89, 0XFFFF);
	
	render_line(25, 150, 295, 150, 0XFFFF);
	render_line(25, 151, 295, 151, 0XFFFF);
	render_line(25, 152, 295, 152, 0XFFFF);
	
	char text_top_row[100] = "Welcome to Tic-Tac-Toe!\0";
	display_text(28, 3, text_top_row);
	
	// Top left box 
	char top_left_box_number[10] = "1\0";
	display_text(8, 7, top_left_box_number);
	
	// Top middle box 
	char top_middle_box_number[10] = "2\0";
	display_text(30, 7, top_middle_box_number);
	
	// Top right box 
	char top_right_box_number[10] = "3\0";
	display_text(53, 7, top_right_box_number);
	
	// Middle left box 
	char middle_left_box_number[10] = "4\0";
	display_text(8, 24, middle_left_box_number);
	
	// Middle middle box 
	char middle_middle_box_number[10] = "5\0";
	display_text(30, 24, middle_middle_box_number);
	
	// Middle right box 
	char middle_right_box_number[10] = "6\0";
	display_text(53, 24, middle_right_box_number);
	
	// Bottom left box 
	char bottom_left_box_number[10] = "7\0";
	display_text(8, 39, bottom_left_box_number);
	
	// Bottom middle box 
	char bottom_middle_box_number[10] = "8\0";
	display_text(30, 39, bottom_middle_box_number);
	
	// Bottom right box 
	char bottom_right_box_number[10] = "9\0";
	display_text(53, 39, bottom_right_box_number);
	
	char winner_status[50] = "Press [H] for help screen.";
	display_text(5, 57, winner_status);
	display_text(50, 57, "Press [B] for Score Board");

}

void display_text(int x, int y, char * text_ptr) {
	int offset;
	volatile char * character_buffer = (char *)0xC9000000; // video character buffer
	
	/* assume that the text string fits on one line */
	offset = (y << 7) + x;
	
	while (*(text_ptr)) {
		*(character_buffer + offset) = *(text_ptr); // write to the character buffer
		++text_ptr;
		++offset;
	}
}


void render_selection_box(int x, int y, short int selection_colour) {
	render_line(x, y, x + 90, y, selection_colour);
	render_line(x + 90, y, x + 90, y + 63, selection_colour);
	render_line(x + 90, y + 63, x, y + 63, selection_colour);
	render_line(x, y + 63, x, y, selection_colour);
}

void render_player(int gameBoardIndex){
	if(Turn == 'X'){
		render_player_X(gameBoardIndex);
	} else {
		render_player_O(gameBoardIndex);
	}
}

void render_player_X(int gameBoardIndex){
	// left diagonal coordinates
	int initial_left_X0 = 29, initial_left_Y0 = 29, initial_left_X1 = 111, initial_left_Y1 = 84;
	// right diagonal coordinates
	int initial_right_X0 = 111, initial_right_Y0 = 29, initial_right_X1 = 29, initial_right_Y1 = 84;
	
	if(gameBoardIndex == 1){
		render_line(initial_left_X0, initial_left_Y0, initial_left_X1, initial_left_Y1, 0xFFFF);
		render_line(initial_right_X0, initial_right_Y0, initial_right_X1, initial_right_Y1, 0xFFFF);
		
	} else if(gameBoardIndex == 2){
		render_line(initial_left_X0 + 90, initial_left_Y0, initial_left_X1 + 90, initial_left_Y1, 0xFFFF);
		render_line(initial_right_X0 + 90, initial_right_Y0, initial_right_X1 + 90, initial_right_Y1, 0xFFFF);	
		
	} else if(gameBoardIndex == 3){
		render_line(initial_left_X0 + 180, initial_left_Y0, initial_left_X1 + 180, initial_left_Y1, 0xFFFF);
		render_line(initial_right_X0 + 180, initial_right_Y0, initial_right_X1 + 180, initial_right_Y1, 0xFFFF);	
		
	} else if(gameBoardIndex == 4){
		render_line(initial_left_X0, initial_left_Y0 + 63, initial_left_X1, initial_left_Y1 + 63, 0xFFFF);
		render_line(initial_right_X0, initial_right_Y0 + 63, initial_right_X1, initial_right_Y1 + 63, 0xFFFF);
	
	} else if(gameBoardIndex == 5){
		render_line(initial_left_X0 + 90, initial_left_Y0 + 63, initial_left_X1 + 90, initial_left_Y1 + 63, 0xFFFF);
		render_line(initial_right_X0 + 90, initial_right_Y0 + 63, initial_right_X1 + 90, initial_right_Y1 + 65, 0xFFFF);
		
	} else if(gameBoardIndex == 6){
		render_line(initial_left_X0 + 180, initial_left_Y0 + 63, initial_left_X1 + 180, initial_left_Y1 + 63, 0xFFFF);
		render_line(initial_right_X0 + 180, initial_right_Y0 + 63, initial_right_X1 + 180, initial_right_Y1 + 63, 0xFFFF);
		
	} else if(gameBoardIndex == 7){
		render_line(initial_left_X0, initial_left_Y0 + 126, initial_left_X1, initial_left_Y1 + 126, 0xFFFF);
		render_line(initial_right_X0, initial_right_Y0 + 126, initial_right_X1, initial_right_Y1 + 126, 0xFFFF);
		
	} else if(gameBoardIndex == 8){
		render_line(initial_left_X0 + 90, initial_left_Y0 + 126, initial_left_X1 + 90, initial_left_Y1 + 126, 0xFFFF);
		render_line(initial_right_X0 + 90, initial_right_Y0 + 126, initial_right_X1 + 90, initial_right_Y1 + 126, 0xFFFF);
		
	} else if(gameBoardIndex == 9){
		render_line(initial_left_X0 + 180, initial_left_Y0 + 126, initial_left_X1 + 180, initial_left_Y1 + 126, 0xFFFF);
		render_line(initial_right_X0 + 180, initial_right_Y0 + 126, initial_right_X1 + 180, initial_right_Y1 + 126, 0xFFFF);
	}
}
	
void render_player_O(int gameBoardIndex){
	int UpLine_R_X = 98, UpLine_R_Y = 27, UpLine_L_X = 42, UpLine_L_Y = 27;
	int BottomLine_R_X = 98, BottomLine_R_Y = 86, BottomLine_L_X = 42, BottomLine_L_Y = 86;
	
	int LeftLine_U_X = 30, LeftLine_U_Y = 31, LeftLine_B_X = 30, LeftLine_B_Y = 82; 
	int RightLine_U_X = 110, RightLine_U_Y = 31, RightLine_B_X = 110, RightLine_B_Y = 82; 
	
	if(gameBoardIndex == 1){
		render_line(UpLine_R_X, UpLine_R_Y, UpLine_L_X, UpLine_L_Y, 0xFFFF);
		render_line(UpLine_L_X, UpLine_L_Y, LeftLine_U_X, LeftLine_U_Y, 0xFFFF);
		render_line(LeftLine_U_X, LeftLine_U_Y, LeftLine_B_X, LeftLine_B_Y, 0xFFFF);
		render_line(LeftLine_B_X, LeftLine_B_Y, BottomLine_L_X, BottomLine_L_Y, 0xFFFF);
		render_line(BottomLine_L_X, BottomLine_L_Y, BottomLine_R_X, BottomLine_R_Y, 0xFFFF);
		render_line(BottomLine_R_X, BottomLine_R_Y, RightLine_B_X, RightLine_B_Y, 0xFFFF);
		render_line(RightLine_B_X, RightLine_B_Y, RightLine_U_X, RightLine_U_Y, 0xFFFF);
		render_line(RightLine_U_X, RightLine_U_Y, UpLine_R_X, UpLine_R_Y, 0xFFFF);
		
	} else if(gameBoardIndex == 2){
		render_line(UpLine_R_X + 90, UpLine_R_Y, UpLine_L_X + 90, UpLine_L_Y, 0xFFFF);
		render_line(UpLine_L_X + 90, UpLine_L_Y, LeftLine_U_X + 90, LeftLine_U_Y, 0xFFFF);
		render_line(LeftLine_U_X + 90, LeftLine_U_Y, LeftLine_B_X + 90, LeftLine_B_Y, 0xFFFF);
		render_line(LeftLine_B_X + 90, LeftLine_B_Y, BottomLine_L_X + 90, BottomLine_L_Y, 0xFFFF);
		render_line(BottomLine_L_X + 90, BottomLine_L_Y, BottomLine_R_X + 90, BottomLine_R_Y, 0xFFFF);
		render_line(BottomLine_R_X + 90, BottomLine_R_Y, RightLine_B_X + 90, RightLine_B_Y, 0xFFFF);
		render_line(RightLine_B_X + 90, RightLine_B_Y, RightLine_U_X + 90, RightLine_U_Y, 0xFFFF);
		render_line(RightLine_U_X + 90, RightLine_U_Y, UpLine_R_X + 90, UpLine_R_Y, 0xFFFF);
		
	} else if(gameBoardIndex == 3){
		render_line(UpLine_R_X + 180, UpLine_R_Y, UpLine_L_X + 180, UpLine_L_Y, 0xFFFF);
		render_line(UpLine_L_X + 180, UpLine_L_Y, LeftLine_U_X + 180, LeftLine_U_Y, 0xFFFF);
		render_line(LeftLine_U_X + 180, LeftLine_U_Y, LeftLine_B_X + 180, LeftLine_B_Y, 0xFFFF);
		render_line(LeftLine_B_X + 180, LeftLine_B_Y, BottomLine_L_X + 180, BottomLine_L_Y, 0xFFFF);
		render_line(BottomLine_L_X + 180, BottomLine_L_Y, BottomLine_R_X + 180, BottomLine_R_Y, 0xFFFF);
		render_line(BottomLine_R_X + 180, BottomLine_R_Y, RightLine_B_X + 180, RightLine_B_Y, 0xFFFF);
		render_line(RightLine_B_X + 180, RightLine_B_Y, RightLine_U_X + 180, RightLine_U_Y, 0xFFFF);
		render_line(RightLine_U_X + 180, RightLine_U_Y, UpLine_R_X + 180, UpLine_R_Y, 0xFFFF);
				
	} else if(gameBoardIndex == 4){
		render_line(UpLine_R_X, UpLine_R_Y + 63, UpLine_L_X, UpLine_L_Y + 63, 0xFFFF);
		render_line(UpLine_L_X, UpLine_L_Y + 63, LeftLine_U_X, LeftLine_U_Y + 63, 0xFFFF);
		render_line(LeftLine_U_X, LeftLine_U_Y + 63, LeftLine_B_X, LeftLine_B_Y + 63, 0xFFFF);
		render_line(LeftLine_B_X, LeftLine_B_Y + 63, BottomLine_L_X, BottomLine_L_Y + 63, 0xFFFF);
		render_line(BottomLine_L_X, BottomLine_L_Y + 63, BottomLine_R_X, BottomLine_R_Y + 63, 0xFFFF);
		render_line(BottomLine_R_X, BottomLine_R_Y + 63, RightLine_B_X, RightLine_B_Y + 63, 0xFFFF);
		render_line(RightLine_B_X, RightLine_B_Y + 63, RightLine_U_X, RightLine_U_Y + 63, 0xFFFF);
		render_line(RightLine_U_X, RightLine_U_Y + 63, UpLine_R_X, UpLine_R_Y + 63, 0xFFFF);

	} else if(gameBoardIndex == 5){
		render_line(UpLine_R_X + 90, UpLine_R_Y + 63, UpLine_L_X + 90, UpLine_L_Y + 63, 0xFFFF);
		render_line(UpLine_L_X + 90, UpLine_L_Y + 63, LeftLine_U_X + 90, LeftLine_U_Y + 63, 0xFFFF);
		render_line(LeftLine_U_X + 90, LeftLine_U_Y + 63, LeftLine_B_X + 90, LeftLine_B_Y + 63, 0xFFFF);
		render_line(LeftLine_B_X + 90, LeftLine_B_Y + 63, BottomLine_L_X + 90, BottomLine_L_Y + 63, 0xFFFF);
		render_line(BottomLine_L_X + 90, BottomLine_L_Y + 63, BottomLine_R_X + 90, BottomLine_R_Y + 63, 0xFFFF);
		render_line(BottomLine_R_X + 90, BottomLine_R_Y + 63, RightLine_B_X + 90, RightLine_B_Y + 63, 0xFFFF);
		render_line(RightLine_B_X + 90, RightLine_B_Y + 63, RightLine_U_X + 90, RightLine_U_Y + 63, 0xFFFF);
		render_line(RightLine_U_X + 90, RightLine_U_Y + 63, UpLine_R_X + 90, UpLine_R_Y + 63, 0xFFFF);
	
	} else if(gameBoardIndex == 6){
		render_line(UpLine_R_X + 180, UpLine_R_Y + 63, UpLine_L_X + 180, UpLine_L_Y + 63, 0xFFFF);
		render_line(UpLine_L_X + 180, UpLine_L_Y + 63, LeftLine_U_X + 180, LeftLine_U_Y + 63, 0xFFFF);
		render_line(LeftLine_U_X + 180, LeftLine_U_Y + 63, LeftLine_B_X + 180, LeftLine_B_Y + 63, 0xFFFF);
		render_line(LeftLine_B_X + 180, LeftLine_B_Y + 63, BottomLine_L_X + 180, BottomLine_L_Y + 63, 0xFFFF);
		render_line(BottomLine_L_X + 180, BottomLine_L_Y + 63, BottomLine_R_X + 180, BottomLine_R_Y + 63, 0xFFFF);
		render_line(BottomLine_R_X + 180, BottomLine_R_Y + 63, RightLine_B_X + 180, RightLine_B_Y + 63, 0xFFFF);
		render_line(RightLine_B_X + 180, RightLine_B_Y + 63, RightLine_U_X + 180, RightLine_U_Y + 63, 0xFFFF);
		render_line(RightLine_U_X + 180, RightLine_U_Y + 63, UpLine_R_X + 180, UpLine_R_Y + 63, 0xFFFF);
		
	} else if(gameBoardIndex == 7){
		render_line(UpLine_R_X, UpLine_R_Y + 126, UpLine_L_X, UpLine_L_Y + 126, 0xFFFF);
		render_line(UpLine_L_X, UpLine_L_Y + 126, LeftLine_U_X, LeftLine_U_Y + 126, 0xFFFF);
		render_line(LeftLine_U_X, LeftLine_U_Y + 126, LeftLine_B_X, LeftLine_B_Y + 126, 0xFFFF);
		render_line(LeftLine_B_X, LeftLine_B_Y + 126, BottomLine_L_X, BottomLine_L_Y + 126, 0xFFFF);
		render_line(BottomLine_L_X, BottomLine_L_Y + 126, BottomLine_R_X, BottomLine_R_Y + 126, 0xFFFF);
		render_line(BottomLine_R_X, BottomLine_R_Y + 126, RightLine_B_X, RightLine_B_Y + 126, 0xFFFF);
		render_line(RightLine_B_X, RightLine_B_Y + 126, RightLine_U_X, RightLine_U_Y + 126, 0xFFFF);
		render_line(RightLine_U_X, RightLine_U_Y + 126, UpLine_R_X, UpLine_R_Y + 126, 0xFFFF);
		
	} else if(gameBoardIndex == 8){
		render_line(UpLine_R_X + 90, UpLine_R_Y + 126, UpLine_L_X + 90, UpLine_L_Y + 126, 0xFFFF);
		render_line(UpLine_L_X + 90, UpLine_L_Y + 126, LeftLine_U_X + 90, LeftLine_U_Y + 126, 0xFFFF);
		render_line(LeftLine_U_X + 90, LeftLine_U_Y + 126, LeftLine_B_X + 90, LeftLine_B_Y + 126, 0xFFFF);
		render_line(LeftLine_B_X + 90, LeftLine_B_Y + 126, BottomLine_L_X + 90, BottomLine_L_Y + 126, 0xFFFF);
		render_line(BottomLine_L_X + 90, BottomLine_L_Y + 126, BottomLine_R_X + 90, BottomLine_R_Y + 126, 0xFFFF);
		render_line(BottomLine_R_X + 90, BottomLine_R_Y + 126, RightLine_B_X + 90, RightLine_B_Y + 126, 0xFFFF);
		render_line(RightLine_B_X + 90, RightLine_B_Y + 126, RightLine_U_X + 90, RightLine_U_Y + 126, 0xFFFF);
		render_line(RightLine_U_X + 90, RightLine_U_Y + 126, UpLine_R_X + 90, UpLine_R_Y + 126, 0xFFFF);
		
	} else if(gameBoardIndex == 9){
		render_line(UpLine_R_X + 180, UpLine_R_Y + 126, UpLine_L_X + 180, UpLine_L_Y + 126, 0xFFFF);
		render_line(UpLine_L_X + 180, UpLine_L_Y + 126, LeftLine_U_X + 180, LeftLine_U_Y + 126, 0xFFFF);
		render_line(LeftLine_U_X + 180, LeftLine_U_Y + 126, LeftLine_B_X + 180, LeftLine_B_Y + 126, 0xFFFF);
		render_line(LeftLine_B_X + 180, LeftLine_B_Y + 126, BottomLine_L_X + 180, BottomLine_L_Y + 126, 0xFFFF);
		render_line(BottomLine_L_X + 180, BottomLine_L_Y + 126, BottomLine_R_X + 180, BottomLine_R_Y + 126, 0xFFFF);
		render_line(BottomLine_R_X + 180, BottomLine_R_Y + 126, RightLine_B_X + 180, RightLine_B_Y + 126, 0xFFFF);
		render_line(RightLine_B_X + 180, RightLine_B_Y + 126, RightLine_U_X + 180, RightLine_U_Y + 126, 0xFFFF);
		render_line(RightLine_U_X + 180, RightLine_U_Y + 126, UpLine_R_X + 180, UpLine_R_Y + 126, 0xFFFF);
	}
}

void start_screen(){
	int offset = 20, offset2 = 15;
	volatile int x,y;
	for (x = 0; x < 320; x++){
		for ( y = 0; y < 240; y++){
			draw_pixel(x, y, 0x00FF);
		}
	}
 // Calculating the centers for each string
    int welcome_center = (80 - strlen("WELCOME")) / 2;
    int to_the_center = (80 - strlen("TO THE")) / 2;
    int tic_tac_toe_center = (80 - strlen("TIC-TAC-TOE")) / 2;
    int game_center = (80 - strlen("GAME!")) / 2;
    int press_enter_center = (80 - strlen("PRESS [X] TO START")) / 2;
    int by_omar_center = (80 - strlen("BY: OMAR ELTERRAS & SARAH HANIYA")) / 2;
	int ECE_center = (80 - strlen("ECE 4436 PROJECT")) / 2;


    // Now write the centered text using display_text
    display_text(welcome_center, 20, "WELCOME");
    display_text(to_the_center, 22, "TO THE");
    display_text(tic_tac_toe_center, 24, "TIC-TAC-TOE");
    display_text(game_center, 26, "GAME!");
    display_text(press_enter_center, 38, "PRESS [X] TO START");
	display_text(ECE_center, 52, "ECE 4436 PROJECT");
    display_text(by_omar_center, 54, "BY: OMAR ELTERRAS & SARAH HANIYA");

}

// Functions checks every possibly win (3 in a row) for either player and returns the winner
int check_winner(){
	// First Column Win
    if(1 == gameBoard[0] && 1 == gameBoard[3] && 1 == gameBoard[6]){
		render_line(69, 25, 69, 214, 0xF800);
		render_line(70, 25, 70, 214, 0xF800);
		render_line(71, 25, 71, 214, 0xF800);
        return 1;
    }

    if(2 == gameBoard[0] && 2 == gameBoard[3] && 2 == gameBoard[6]){
		render_line(69, 25, 69, 214, 0xF800);
		render_line(70, 25, 70, 214, 0xF800);
		render_line(71, 25, 71, 214, 0xF800);
        return 2;
    }
	
	// Second Column Win 
    if(1 == gameBoard[1] && 1 == gameBoard[4] && 1 == gameBoard[7]){
		render_line(159, 25, 159, 214, 0xF800);
		render_line(160, 25, 160, 214, 0xF800);
		render_line(161, 25, 161, 214, 0xF800);
        return 1;
    }

    if(2 == gameBoard[1] && 2 == gameBoard[4] && 2 == gameBoard[7]){
		render_line(159, 25, 159, 214, 0xF800);
		render_line(160, 25, 160, 214, 0xF800);
		render_line(161, 25, 161, 214, 0xF800);
        return 2;
    }
	
	// Third column win 
	if(1 == gameBoard[2] && 1 == gameBoard[5] && 1 == gameBoard[8]){
		render_line(249, 25, 249, 214, 0xF800);
		render_line(250, 25, 250, 214, 0xF800);
		render_line(251, 25, 251, 214, 0xF800);
        return 1;
    }

    if(2 == gameBoard[2] && 2 == gameBoard[5] && 2 == gameBoard[8]){
		render_line(249, 25, 249, 214, 0xF800);
		render_line(250, 25, 250, 214, 0xF800);
		render_line(251, 25, 251, 214, 0xF800);        
		return 2;
    }
	
	// First row Win
    if(1 == gameBoard[0] && 1 == gameBoard[1] && 1 == gameBoard[2]){
		render_line(25, 55, 295, 55, 0xF800);
		render_line(25, 56, 295, 56, 0xF800);
		render_line(25, 57, 295, 57, 0xF800);
        return 1;
    }

    if(2 == gameBoard[0] && 2 == gameBoard[1] && 2 == gameBoard[2]){
		render_line(25, 55, 295, 55, 0xF800);
		render_line(25, 56, 295, 56, 0xF800);
		render_line(25, 57, 295, 57, 0xF800);
        return 2;
    }
	
	// Second row Win 
    if(1 == gameBoard[3] && 1 == gameBoard[4] && 1 == gameBoard[5]){
		render_line(25, 118, 295, 118, 0xF800);
		render_line(25, 119, 295, 119, 0xF800);
		render_line(25, 120, 295, 120, 0xF800);
        return 1;
    }

    if(2 == gameBoard[3] && 2 == gameBoard[4] && 2 == gameBoard[5]){
		render_line(25, 118, 295, 118, 0xF800);
		render_line(25, 119, 295, 119, 0xF800);
		render_line(25, 120, 295, 120, 0xF800);
        return 2;
    }
	
	// Third row win 
	if(1 == gameBoard[6] && 1 == gameBoard[7] && 1 == gameBoard[8]){
		render_line(25, 181, 295, 181, 0xF800);
		render_line(25, 182, 295, 182, 0xF800);
		render_line(25, 183, 295, 183, 0xF800);
        return 1;
    }

    if(2 == gameBoard[6] && 2 == gameBoard[7] && 2 == gameBoard[8]){
		render_line(25, 181, 295, 181, 0xF800);
		render_line(25, 182, 295, 182, 0xF800);
		render_line(25, 183, 295, 183, 0xF800);
        return 2;
    }
	
	// Left diagonal win
    if(1 == gameBoard[0] && 1 == gameBoard[4] && 1 == gameBoard[8]){
		render_line(24, 24, 294, 213, 0xF800);
		render_line(25, 25, 295, 214, 0xF800);
		render_line(26, 26, 296, 215, 0xF800);
        return 1;
    }

    if(2 == gameBoard[0] && 2 == gameBoard[4] && 2 == gameBoard[8]){
		render_line(24, 24, 294, 213, 0xF800);
		render_line(25, 25, 295, 214, 0xF800);
		render_line(26, 26, 296, 215, 0xF800);
        return 2;
    }
    
	// Right diagonal win
    if(1 == gameBoard[2] && 1 == gameBoard[4] && 1 == gameBoard[6]){
		render_line(294, 24, 24, 213, 0xF800);
		render_line(295, 25, 25, 214, 0xF800);
		render_line(296, 26, 26, 215, 0xF800);
        return 1;
    }
    
    if (2 == gameBoard[2] && 2 == gameBoard[4] && 2 == gameBoard[6]){
		render_line(294, 24, 24, 213, 0xF800);
		render_line(295, 25, 25, 214, 0xF800);
		render_line(296, 26, 26, 215, 0xF800);
        return 2;
    }
	
	checkforDraw();
	if (isDraw){
		return 3;
	}
	
	return 0;
}

// Checks if every position has been filled
void checkforDraw(){
	volatile index = 0;
    for( index = 0; index < 9; index++){
        // 0 means no one has claimed that position
        if(gameBoard[index] == 0){
            isDraw = false;
			return;
        }
    }
	isDraw = true;
}

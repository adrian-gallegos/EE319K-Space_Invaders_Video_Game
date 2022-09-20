; Print.s
; Student names: change this to your names or look very silly
; Last modification date: change this to the last modification date or look very silly
; Runs on TM4C123
; EE319K lab 7 device driver for any LCD
;
; As part of Lab 7, students need to implement these LCD_OutDec and LCD_OutFix
; This driver assumes two low-level LCD functions
; SSD1306_OutChar   outputs a single 8-bit ASCII character
; SSD1306_OutString outputs a null-terminated string 

    IMPORT   SSD1306_OutChar
    IMPORT   SSD1306_OutString
    EXPORT   LCD_OutDec
    EXPORT   LCD_OutFix
    PRESERVE8
    AREA    |.text|, CODE, READONLY, ALIGN=2
    THUMB

N EQU 4
CNT EQU 0
FP	RN 11

;-----------------------LCD_OutDec-----------------------
; Output a 32-bit number in unsigned decimal format
; Input: R0 (call by value) 32-bit unsigned number
; Output: none
; Invariables: This function must not permanently modify registers R4 to R11
LCD_OutDec
	PUSH {R4-R11}
; Init1
	PUSH {R0}			; Store number N on stack
	SUB SP, SP, #4		; Make space for count (Subtract SP by 4)
	MOV FP, SP			; Make SP your frame pointer

; Init2
	PUSH {LR}			; Store LR on stack
	MOV R1, #0
	STR R1, [FP,#CNT]	; Store count as 0
	MOV R1, #10			; Load a reg with #10 for division
	
Readloop
; Step 1
	LDR R2, [FP,#CNT]
	ADD R2, #1
	STR R2, [FP,#CNT]
; Step 2
	LDR R3, [FP,#N]		; Load current value of N to reg1 <- R3
	MOV R4, R3			; Make a copy in reg2 <- R4
	UDIV R3, R1			; Unsigned divide reg1 by 10 in reg1
	STR R3, [FP,#N]		; Store this reg1 as new N
	MUL R3, R1			; Multiply this reg1 to 10 in reg1
	SUB R4, R3			; R4-R3
	PUSH {R4}			; Store this difference to stack
; Step 3
	LDR R3, [FP,#N]		; Load new N
	CMP R3, #0			; Check if N is 0
	BNE Readloop
Writeloop
; Step 4
	MOV R5, SP
	POP {R5}			; POP value from stack
	ADD R0, R5, #0x30	; Add 0x30 b/c of ASCII char
	BL	SSD1306_OutChar
; Step 5
	LDR R3, [FP, #CNT]
	SUB R3, #1
	STR R3, [FP, #CNT]
; Step 6
	CMP R3, #0
	BNE Writeloop
; Step 7
	POP {LR}
	ADD SP, #8

	POP {R4-R11}
    BX  LR
;* * * * * * * * End of LCD_OutDec * * * * * * * *

; -----------------------LCD _OutFix----------------------
; Output characters to LCD display in fixed-point format
; unsigned decimal, resolution 0.01, range 0.00 to 9.99
; Inputs:  R0 is an unsigned 32-bit number
; Outputs: none
; E.g., R0=0,    then output "0.00 "
;       R0=3,    then output "0.03 "
;       R0=89,   then output "0.89 "
;       R0=123,  then output "1.23 "
;       R0=999,  then output "9.99 "
;       R0>999,  then output "*.** "
; Invariables: This function must not permanently modify registers R4 to R11
LCD_OutFix
	PUSH {R4-R11}
	
; Init1
	PUSH {R0}			; Store number N on stack
	SUB SP, SP, #4		; Make space for count (Subtract SP by 4)
	MOV FP, SP			; Make SP your frame pointer

; Init2
	PUSH {LR}			; Store LR on stack
	MOV R1, #0
	STR R1, [FP,#CNT]	; Store count as 0
	MOV R1, #10			; Load a reg with #10 for division
	
; Step 1
	LDR R2, [FP,#N]
	CMP R2, #1000
	BLO InRange
; Step 2
	MOV R0, #0x2A
	BL SSD1306_OutChar
; Step 3
	MOV R0, #0x2E
	BL SSD1306_OutChar
; Step 4
	MOV R0, #0x2A
	BL SSD1306_OutChar
; Step 5
	MOV R0, #0x2A
	BL SSD1306_OutChar
	B	Exit
InRange
; Step 6
	LDR R2, [FP,#CNT]
	ADD R2, #1
	STR R2, [FP,#CNT]
; Step 7
	LDR R3, [FP,#N]		; Load current value of N to reg1 <- R3
	MOV R4, R3			; Make a copy in reg2 <- R4
	UDIV R3, R1			; Unsigned divide reg1 by 10 in reg1
	STR R3, [FP,#N]		; Store this reg1 as new N
	MUL R3, R1			; Multiply this reg1 to 10 in reg1
	SUB R4, R3			; R4-R3
	PUSH {R4}			; Store this difference to stack
; Step 8
	LDR R2, [FP,#CNT]
	CMP R2, #3
	BLO InRange
; Step 9
	MOV R5, SP
	POP {R5}			; POP value from stack
	ADD R0, R5, #0x30	; Add 0x30 b/c of ASCII char
	BL	SSD1306_OutChar
	
	MOV R0, #0x2E
	BL SSD1306_OutChar
	
	MOV R5, SP
	POP {R5}			; POP value from stack
	ADD R0, R5, #0x30	; Add 0x30 b/c of ASCII char
	BL	SSD1306_OutChar
	
	MOV R5, SP
	POP {R5}			; POP value from stack
	ADD R0, R5, #0x30	; Add 0x30 b/c of ASCII char
	BL	SSD1306_OutChar
	
Exit	
; Step 10
	POP {LR}
	ADD SP, #8

	POP {R4-R11}
    BX   LR
 
    ;ALIGN
;* * * * * * * * End of LCD_OutFix * * * * * * * *

     ALIGN          ; make sure the end of this section is aligned
     END            ; end of file

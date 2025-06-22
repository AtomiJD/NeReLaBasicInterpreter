import HTTP

' --- Simple JSON Error Handler ---
SUB HandleJsonError()
    PRINT
    PRINT "!!!!!!!!!!!!!!!!!!!!!!!!!!"
    PRINT "! JSON PARSING FAILED!   !"
    PRINT "!!!!!!!!!!!!!!!!!!!!!!!!!!"
    PRINT "Error Code: "; ERR
    PRINT "Line Number: "; ERL
    PRINT "This usually means OpenAI returned an error message instead of a valid chat completion."
    PRINT "Raw response from server:"
    PRINT RESPONSE$
    RESUME NEXT ' Skip the failed parsing and continue the loop
ENDSUB

' --- Simple String Replace Function ---
FUNC ReplaceString(source$, find$, replace$)
    result$ = ""
    start_pos = 1
    exit_do = 0	
loop_start:
        found_pos = INSTR(start_pos, source$, find$)
        if found_pos = 0 then
            result$ = result$ + MID$(source$, start_pos)
            exit_do = 1
	    goto loop_ende
        endif
        result$ = result$ + MID$(source$, start_pos, found_pos - start_pos) + replace$
        start_pos = found_pos + LEN(find$)
loop_ende:
    If exit_do = 0 then GOTO loop_start
    RETURN result$
ENDFUNC


REM --- NeReLa Basic OpenAI Chat Client with JSON Parsing ---
REM This program uses the HTTP.POST$ function to send a prompt
REM to the OpenAI Chat Completions API and then parses the
REM JSON response to extract the message content.

CLS
PRINT "--- OpenAI API Chat Client for NeReLa Basic ---"
PRINT

API_KEY$ = Getenv$("OPENAI_API_KEY")

IF LEN(API_KEY$) = 0 THEN
    PRINT "Error: OPENAI_API_KEY environment variable not set."
    GOTO ENDE
ENDIF

REM The API endpoint for chat completions
API_URL$ = "https://api.openai.com/v1/chat/completions"

REM Set the required HTTP headers for the OpenAI API
HTTP.SETHEADER "Content-Type", "application/json"
HTTP.SETHEADER "Authorization", "Bearer " + API_KEY$

PRINT
PRINT "Headers are set. You can now start chatting."
PRINT "Type 'exit' or 'quit' to end the program."
PRINT

DO
    PRINT
    INPUT "> "; USER_PROMPT$

    REM Check for exit condition
    IF LCASE$(USER_PROMPT$) = "exit" OR LCASE$(USER_PROMPT$) = "quit" THEN
        PRINT "Goodbye!"
        GOTO ENDE
    ENDIF

    PRINT "Sending to OpenAI..."

    REM --- Construct the JSON payload ---
    Q$ = CHR$(34)
    JSON_BODY$ = "{ "
    JSON_BODY$ = JSON_BODY$ + Q$ + "model" + Q$ + ": " + Q$ + "gpt-4o-mini" + Q$ + ", "
    JSON_BODY$ = JSON_BODY$ + Q$ + "messages" + Q$ + ": [{"
    JSON_BODY$ = JSON_BODY$ + Q$ + "role" + Q$ + ": " + Q$ + "user" + Q$ + ", "
    REM --- A simple function to escape quotes in the user's prompt ---
    USER_PROMPT$ = ReplaceString(USER_PROMPT$, Q$, "\\" + Q$)
    JSON_BODY$ = JSON_BODY$ + Q$ + "content" + Q$ + ": " + Q$ + USER_PROMPT$ + Q$
    JSON_BODY$ = JSON_BODY$ + "}]"
    JSON_BODY$ = JSON_BODY$ + " }"

    REM --- Make the API call using HTTP.POST$ ---
    RESPONSE$ = HTTP.POST$(API_URL$, JSON_BODY$,"Content-Type: application/json")

    PRINT "--- Parsing JSON Response ---"

    REM --- Use our new JSON.PARSE$ function! ---
    ON ERROR CALL HandleJsonError
    REM --- Access the data using the new syntax ---
    REM The structure is: { "choices": [ { "message": { "content": "..." } } ] }

    RESPONSE_JSON = JSON.PARSE$(RESPONSE$)

    'A =  RESPONSE_JSON{"choices"}    
    'b = a[0]
    'c = b{"message"}
    'AI_MESSAGE$ = c{"content"}

    AI_MESSAGE$=RESPONSE_JSON{"choices"}[0]{"message"}{"content"}
    
    PRINT
    PRINT "AI: "; AI_MESSAGE$

LOOP WHILE 1 ' Loop forever until user types exit/quit


ENDE:
Print "Bye."

PRINT "--- Testing TXTREADER$ ---"
MY_STORY$ = TXTREADER$("story.txt")
PRINT "The story file contains:"
PRINT MY_STORY$
PRINT "The story file split in lines:"
V = SPLIT(MY_STORY$,chr$(10))
PRINT "There are "; len(v); " lines:"
print "This is line 2: \n"; v[1]





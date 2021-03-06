#ifdef DELTA
#include "delta.h"
#endif
#include "tm_lib.h"

#define FORMULAFILE "formula"
#define INPUTFILE "input_string"
#define INPUTCLEANFILE "input_string_clean"
#define STATESFILE "state_list"
#ifdef SLOW
#define SLEEP_TIME SLOW*1000 //convert to microseconds
#endif

#define clearConsole() printf("\e[1;1H\e[2J")
#define isState(X) (X >= prop->alphabet_length)
#define isStateWindow(X) (X < 0)
#define encodeStateId(X) (-(X+1)) //must encode/decode as index in the states array
#define decodeStateId(X) (abs(X)-1) 
#define isAccOrRejState(X) (isStateWindow(X) && decodeStateId(X)+prop->alphabet_length >= cell_sym_len-2)

//Macros to write in the window list
#define writeTopRow() win_ptr->window[0]=perm_ptr->permutation[0];win_ptr->window[1]=perm_ptr->permutation[1];win_ptr->window[2]=perm_ptr->permutation[2]
#define writeBottomRow(A, B, C) win_ptr->window[3]=A;win_ptr->window[4]=B;win_ptr->window[5]=C
#define writeWindow(A, B, C) writeTopRow();writeBottomRow(A, B, C);win_ptr=add_window(win_ptr)

//Macros to write in the formula file
#define writeLiteralId(I, J, X) fprintf(formula, "%li ", literal_id(I, J, X, prop));literals++
#define writeLiteralIdNegated(I, J, X) fprintf(formula, "-%li ", literal_id(I, J, X, prop));literals++
#define writeFreshLiteral(X) fprintf(formula, "%li ", X);literals++
#define writeFreshLiteralNegated(X) fprintf(formula, "-%li ", X);literals++
#define writeWindowElement(I, J, N) writeLiteralId(I, J, (isStateWindow(win_ptr->window[N]) ? \
                                    decodeStateId(win_ptr->window[N])+prop->alphabet_length : \
                                    symbol_id(win_ptr->window[N], prop))) //TODO maybe surround multiple statements with {}
#define endClause() fprintf(formula, "0\n");clauses++                     //TODO add unsigneds and add proper assignements to structs like x = {..., ..., ...}

//Removes whitespace and newlines
static int normalize_input(FILE * dest, FILE * src);
//Given a string src writes a string of all unique chars in dest
static void get_alphabet(char * dest, char * src, int len);

#ifdef FORMULA
//Returns unique literal id that identifies the symbol s_id in the cell [i,j]
static long literal_id(int i, int j, int s_id, tm_properties * prop);
//Returns state index in the states array
static int state_id(int s, tm_properties * prop);
//Returns symbol index in the alphabet array
static int symbol_id(char c, tm_properties * prop);
//The highest literal id is in the bottom right corner, it will always be the accept state
static long max_literaL_id(tm_properties * prop);

//Using legal permutations and delta, generates a list of all possible legal windows
static void calculate_legaL_windows(window_node * legal_windows, tm_properties * prop);
//Generates a list of all possible permutations in the upper window row, excluding illegal permutations
static void calculate_permutations(permutation_node * l, tm_properties * prop);
#endif

int main(int argc, char const *argv[]){
    FILE * input = fopen(INPUTFILE, "r");
    FILE * input_clean = fopen(INPUTCLEANFILE, "w+");
    FILE * state_list = fopen(STATESFILE, "r");

    transition * tr = malloc(sizeof(transition));
    char_node * tape = malloc(sizeof(char_node));
    char_node * head;
    tm_properties * prop; //data needed to generate formula
    char curr_symbol, c;
    char * str_state_num = malloc((MAX_INT_DIGITS+1)*sizeof(char));
    int i, j, len, curr_state, curr_steps = 0, head_offset = 0;
    bool inputError = false;

    tape->prev = tape;
    tape->next = NULL;
    head = tape;

    read_int(state_list, str_state_num);
    curr_state = atoi(str_state_num); //initial state
    rewind(state_list);

    if(normalize_input(input_clean, input) != -1){ //remove whitespace and newlines
        c = fgetc(input_clean);
        head->elem = c;
        curr_symbol = c;

        while((c = fgetc(input_clean)) != EOF)
            add_char(tape, c);
        rewind(input_clean);

#ifdef FORMULA
        //Calculate input alphabet
        prop = malloc(sizeof(tm_properties));
        len = count_chars(input_clean);
        prop->alphabet = malloc(sizeof(char)*(len+3)); //# and _ are included
        prop->input_string = malloc(sizeof(char)*(len+1));
        tape_to_str(tape, prop->input_string);
        get_alphabet(prop->alphabet, prop->input_string, len); 
        prop->alphabet_length = strlen(prop->alphabet);

        //Get state list
        len = count_lines(state_list)-1;
        prop->states_length = len+2; //-1 and -2 (reject and accept) are included
        prop->states = malloc((len+2)*sizeof(int));
        for(i = 0; i < len; ++i){
            read_int(state_list, str_state_num);
            prop->states[i] = atoi(str_state_num);
        }
        prop->states[i] = -1; 
        prop->states[i+1] = -2;
#endif

        while(tr->move != ACCEPT && tr->move != REJECT && tr->move != ERROR){
            clearConsole();
            printf("Computation steps: %d\n", curr_steps);
            printf("State: %d\n", curr_state);
            print_tape(tape, head_offset);
            tr = delta(curr_state, curr_symbol);
            print_transition(curr_state, curr_symbol, tr);
            curr_state = tr->state;
            head->elem = tr->symbol;

            if(tr->move == LEFT){
                if(head_offset > 0)
                    head_offset--;
                head = head->prev;
            }else if(tr->move == RIGHT){
                if(head->next == NULL)
                    head->next = blank_node(head);
                head_offset++;
                head = head->next;
            }

            if(tr->move != ERROR)
                curr_symbol = head->elem;

            curr_steps++;
            #ifdef SLOW
            usleep(SLEEP_TIME);
            #endif
        }

        switch(tr->move){
            case ACCEPT:
            printf("Input accepted.\n\n");
            break;

            case REJECT:
            printf("Input rejected.\n\n");
            break;

            case ERROR:
            printf("Input rejected, symbol '%c' is not recognized.\n\n", curr_symbol);
            break;

            default:
            printf("Fatal error.\n\n"); //should never get here
            break;
        }
        
    }else{
        printf("Error: '_' and '#' characters are not allowed in input string!\n");
        inputError = true;
    }

    free(tr);
    list_dealloc_char(tape);
    fclose(input);
    fclose(input_clean);
    fclose(state_list);
    system("rm ./tm.out"); //remove this executable
    system("rm " INPUTCLEANFILE ";rm " STATESFILE);

#ifdef FORMULA
    if(!inputError){

        #ifdef STEPS //Use user input as upper bound
            printf("Using %d as maximum amount of steps instead of %d\n\n", STEPS, curr_steps);
            prop->tot_steps = STEPS;
        #else
            prop->tot_steps = curr_steps;
        #endif

        FILE * formula = fopen(FORMULAFILE, "w+");
        print_properties(prop);

        //Calculate the 4 parts of the formula
        int k, h, win_len, input_length = strlen(prop->input_string);
        int cell_sym_len = prop->alphabet_length + prop->states_length; //total number of possible symbols in a cell
        int accept_state_id = cell_sym_len-1, initial_state_id = prop->alphabet_length;
        int sym_state;  //used to write a cell symbol correspoding to a state
        char sym;       //used to write a cell symbol correspoding to an alphabet symbol
        long clauses = 0, literals = 0;

        prop->table_height = prop->tot_steps+1;   //The initial configuration must also be counted
        prop->table_width = prop->table_height+3; //Must count 2 delimiters '#' and 1 state cell
        printf("Tableau size\nHeight: %d\nWidth: %d\n\n", prop->table_height, prop->table_width);
        printf("Computing formula ...\n");

        /* [i,j] identifies the position of each cell
         * Cell symbols = {alphabet}U{states}
         * This set will be implemented like this:
         * cell_sym = "#,_, ...alphabet..., ...states..., -1, -2" where -1 and -2 are reject and accept
         * indexes of this "imaginary" array identify each possible cell symbol
         * these indexes, combined with the cell position [i,j], are used to get the unique literal id in symbol_id()
         * i,j are 1-based, cell_sym is 0-based
         */  

        //Phi-start
        printf("Computing phi-start ...\n");
        fprintf(formula, "p cnf @"); //syntax: "p cnf n_variables n_clauses"
        for(i = 0; i < MAX_LONG_DIGITS*2; ++i) //create space to write these two numbers later
            fprintf(formula, " ");
        fprintf(formula, "\n");
        fprintf(formula, "c ##### Phi-start #####\n");
        writeLiteralId(1, 1, symbol_id('#', prop));
        endClause();
        writeLiteralId(1, 2, initial_state_id);
        endClause();
        fflush(formula);
        for(j = 0; j < input_length; ++j){
            writeLiteralId(1, j+3, symbol_id(prop->input_string[j], prop));
            endClause();
        }
        for(j += 3; j < prop->table_width; ++j){
            writeLiteralId(1, j, symbol_id('_', prop));
            endClause();
        }
        writeLiteralId(1, j, symbol_id('#', prop));
        endClause();
        fflush(formula);

        //Phi-accept
        printf("Computing phi-accept ...\n");
        fprintf(formula, "c ##### Phi-accept #####\n");
        for(i = 1; i <= prop->table_height; ++i){
            for(j = 1; j <= prop->table_width; ++j){
                writeLiteralId(i, j, accept_state_id);
            }
        }
        endClause();
        fflush(formula);

        //Phi-cell 
        fprintf(formula, "c ##### Phi-cell #####\n");
        printf("Computing phi-cell ...\n");
        for(i = 1; i <= prop->table_height; ++i){
            for(j = 1; j <= prop->table_width; ++j){
                fprintf(formula, "c ## Cell [%d,%d] ##\n", i, j);

                for(k = 0; k < cell_sym_len; ++k){
                    writeLiteralId(i, j, k);
                }
                endClause();


                for(k = 0; k < cell_sym_len; ++k){
                    for(h = 0; h < cell_sym_len; ++h){
                        if(h != k){
                            writeLiteralIdNegated(i, j, k);
                            writeLiteralIdNegated(i, j, h);
                            endClause();
                        }
                    }
                }

            }
        }   
        fflush(formula);

        //Phi-move
        fprintf(formula, "c ##### Phi-move #####\n");
        printf("Computing phi-move ...\n");
        window_node * legal_windows = malloc(sizeof(window_node));
        window_node * win_ptr;
        calculate_legaL_windows(legal_windows, prop);

        //Write while converting legal windows in conjunctive normal form
        printf("Converting into cnf ...\n");
        long curr_max_id, dnf_clause_var;

        win_len = list_len_window(legal_windows)-1;
        curr_max_id = max_literaL_id(prop)+1;

        for(i = 1; i < prop->table_height; ++i){
            for(j = 2; j < prop->table_width; ++j){
                fprintf(formula, "c ## Window [%d,%d] ##\n", i, j);

                dnf_clause_var = curr_max_id;
                for(k = 0; k < win_len; ++k){ //each window is a clause of the DNF phi-move, we introduce a new var for each clause
                    writeFreshLiteral(curr_max_id);
                    curr_max_id++;
                }
                endClause();

                win_ptr = legal_windows;
                while(win_ptr->next != NULL){
                    writeFreshLiteralNegated(dnf_clause_var);       
                    writeWindowElement(i, j-1, 0);
                    endClause();

                    writeFreshLiteralNegated(dnf_clause_var);
                    writeWindowElement(i, j, 1);
                    endClause();

                    writeFreshLiteralNegated(dnf_clause_var);
                    writeWindowElement(i, j+1, 2);
                    endClause();

                    writeFreshLiteralNegated(dnf_clause_var);
                    writeWindowElement(i+1, j-1, 3);
                    endClause();

                    writeFreshLiteralNegated(dnf_clause_var);
                    writeWindowElement(i+1, j, 4);
                    endClause();

                    writeFreshLiteralNegated(dnf_clause_var);
                    writeWindowElement(i+1, j+1, 5);
                    endClause();

                    dnf_clause_var++;
                    win_ptr = win_ptr->next;
                }
            }
        }
        fflush(formula);

        //Write variable count followed by clause count at the beginning of the file
        rewind(formula);
        c = fgetc(formula);
        while(c != '@') //used as placeholder
            c = fgetc(formula);
        fseek(formula, -1, SEEK_CUR);
        write_long(formula, curr_max_id-1);
        fprintf(formula, " ");
        write_long(formula, clauses);
        fprintf(formula, "\n");
        if(fgetc(formula) == ' '){
            fseek(formula, -1, SEEK_CUR);
            fprintf(formula, "c");
        }

        printf("Done.\n");
        printf("Literals:%li Variables:%li Clauses:%li\n", literals, curr_max_id-1, clauses);

        list_dealloc_windows(legal_windows);
        fclose(formula);
    }
#endif//FORMULA

    return EXIT_SUCCESS;
}

static int normalize_input(FILE * dest, FILE * src){
    char c;
    while((c = fgetc(src)) != EOF){
        if(c == '#' || c == '_'){
            return -1;
        }else if(c != ' ' && c != '\n'){
            fprintf(dest, "%c", c);
        }
    }
    rewind(dest);

    return 0;
}

static void get_alphabet(char * dest, char * src, int len){
    int i = 0, k = 2;
    dest[0] = '#';
    dest[1] = '_';
    for(i; i < len && src[i] != '\0'; ++i){
        if(!contains(dest, src[i], i+3)){
            dest[k] = src[i];
            k++;
        }
    }
    dest[k] = '\0';
}

#ifdef FORMULA
    /* s_id is an index of this "imaginary" array of all possible cell symbols:
     * cell_sym = "#,_, ...alphabet..., ...states..., -1, -2" where -1 and -2 are reject and accept
     */
    static long literal_id(int i, int j, int s_id, tm_properties * prop){
        int cell_sym_len = prop->alphabet_length + prop->states_length;
        int offset = -cell_sym_len+s_id+1;

        return (i-1)*prop->table_width*cell_sym_len + (j*cell_sym_len) + offset;
    }

    static int state_id(int s, tm_properties * prop){
        for(int i = 0; i < prop->states_length; ++i){
            if (prop->states[i] == s)
                return i;
        }

        return -1;
    }

    static int symbol_id(char c, tm_properties * prop){
        for(int i = 0; i < prop->alphabet_length; ++i){
            if (prop->alphabet[i] == c)
                return i;
        }

        return -1;
    }

    static long max_literaL_id(tm_properties * prop){ 
        int accept_state_id = prop->alphabet_length + prop->states_length - 1;
        return literal_id(prop->table_height, prop->table_width, accept_state_id, prop);
    }

    static void calculate_legaL_windows(window_node * legal_windows, tm_properties * prop){
        window_node * win_ptr = legal_windows;
        int i, j, acc_state_id = prop->states_length-1, rej_state_id = prop->states_length-2; 
        int cell_sym_len = prop->alphabet_length + prop->states_length;
        transition * tr = malloc(sizeof(transition));
        permutation_node * permutations = malloc(sizeof(permutation_node)); 
        permutation_node * perm_ptr = permutations;

        //Permutations and windows are actually integers arrays treated as char arrays, where negative integers are states
        calculate_permutations(permutations, prop);

        //For each permutation: write all possible window bottom rows in newly created windows by using the transition function delta
        while(perm_ptr->next != NULL){

            //CASE 1: There are no states in the permutation
            if(!isStateWindow(perm_ptr->permutation[0]) && 
               !isStateWindow(perm_ptr->permutation[1]) && 
               !isStateWindow(perm_ptr->permutation[2]))
            {
                //'_' in these comments means an information we don't care about
                //Symbols remain the same (trivial)
                writeWindow(perm_ptr->permutation[0], perm_ptr->permutation[1], perm_ptr->permutation[2]);

                //State appears from the left
                //See if there are transitions (_,s[0])->(qx,_,R) 
                for(i = 0; i < prop->states_length-2; ++i){ //exclude accept and reject
                    tr = delta(prop->states[i], perm_ptr->permutation[0]);
                    if(tr->move == RIGHT){
                        writeWindow(encodeStateId(state_id(tr->state, prop)), perm_ptr->permutation[1], perm_ptr->permutation[2]);
                    }
                }

                //Only left symbol is modified
                //See if there are transitions (_,s[0])->(_,s,L)
                for(i = 0; i < prop->states_length-2; ++i){ //exclude accept and reject
                    tr = delta(prop->states[i], perm_ptr->permutation[0]);
                    if(tr->move == LEFT){
                        writeWindow(tr->symbol, perm_ptr->permutation[1], perm_ptr->permutation[2]);
                    }
                }

                //State appears from the right (added exception for delimiter '#')
                //See if there are transitions (_,_)->(qx,_,L)
                if(perm_ptr->permutation[2] != '#'){ //can't appear from the right if there's a delimiter
                    for(i = 0; i < prop->states_length-2; ++i){
                        for(j = 0; j < prop->alphabet_length; ++j){
                            tr = delta(prop->states[i], prop->alphabet[j]);
                            if(tr->move == LEFT){
                                writeWindow(perm_ptr->permutation[0], perm_ptr->permutation[1], encodeStateId(state_id(tr->state, prop)));
                            }
                        }
                    }
                }

            //CASE 2: States are present in the permutation
            }else{
                if(isAccOrRejState(perm_ptr->permutation[0]) ||
                   isAccOrRejState(perm_ptr->permutation[1]) ||
                   isAccOrRejState(perm_ptr->permutation[2]))
                {
                    //Symbols remain the same, this is a special "static" transition for acc and rej
                    writeWindow(perm_ptr->permutation[0], perm_ptr->permutation[1], perm_ptr->permutation[2]);
                }else{

                    //State is in the left side
                    if(isStateWindow(perm_ptr->permutation[0])){
                        tr = delta(prop->states[decodeStateId(perm_ptr->permutation[0])], perm_ptr->permutation[1]);
                        switch(tr->move){
                            //State disappears to the left, unknown symbol appears from the left (but it can't be the delimiter '#')
                            //See if there are transitions (q[0],s[1])->(_,s,L)
                            case LEFT:
                                for(j = 1; j < prop->alphabet_length; ++j){ //start from 1 to exclude delimiter
                                    writeWindow(prop->alphabet[j], tr->symbol, perm_ptr->permutation[2]);   
                                }
                            break;

                            //State moves from left to center
                            //See if there are transitions (q[0],s[1])->(q,s,R)
                            case RIGHT:
                                writeWindow(tr->symbol, encodeStateId(state_id(tr->state, prop)), perm_ptr->permutation[2]); 
                            break;

                            //Acc and rej, See if there are transitions (q[0],s[1])->ACCEPT/REJECT
                            case ACCEPT:
                                writeWindow(encodeStateId(acc_state_id), perm_ptr->permutation[1], perm_ptr->permutation[2]);
                            break;
                            case REJECT:
                                writeWindow(encodeStateId(rej_state_id), perm_ptr->permutation[1], perm_ptr->permutation[2]);
                            break;
                            default:
                            break;
                        }

                    //State is in the center
                    }else if(isStateWindow(perm_ptr->permutation[1])){
                        tr = delta(prop->states[decodeStateId(perm_ptr->permutation[1])], perm_ptr->permutation[2]);
                        switch(tr->move){
                            //State moves from center to left (added exception for delimiter '#')
                            //See if there are transitions (q[1],s[2])->(q,s,L)
                            case LEFT:
                                if(perm_ptr->permutation[0] != '#'){ //can't move to the left if there's a delimiter
                                    writeWindow(encodeStateId(state_id(tr->state, prop)), perm_ptr->permutation[0], tr->symbol); 
                                } 
                            break;

                            //State moves from center to right 
                            //See if there are transitions (q[1],s[2])->(q,s,R)
                            case RIGHT:
                                writeWindow(perm_ptr->permutation[0], tr->symbol, encodeStateId(state_id(tr->state, prop))); 
                            break;

                            //Acc and rej, See if there are transitions (q[1],s[2])->ACCEPT/REJECT
                            case ACCEPT:
                                writeWindow(perm_ptr->permutation[0], encodeStateId(acc_state_id), perm_ptr->permutation[2]);
                            break;
                            case REJECT:
                                writeWindow(perm_ptr->permutation[0], encodeStateId(rej_state_id), perm_ptr->permutation[2]);
                            break;
                            default:
                            break;
                        }

                    //State is in the right side    
                    }else{  
                        for(j = 0; j < prop->alphabet_length; ++j){ 
                            tr = delta(prop->states[decodeStateId(perm_ptr->permutation[2])], prop->alphabet[j]);
                            switch(tr->move){
                                //State moves from right to center
                                //See if there are transitions (q[2],_)->(q,_,L)
                                case LEFT:
                                    writeWindow(perm_ptr->permutation[0], encodeStateId(state_id(tr->state, prop)), perm_ptr->permutation[1]);
                                break;

                                //State disappears to the right, written symbol appears from the right
                                //See if there are transitions (q[2],_)->(_,s,R) 
                                case RIGHT:
                                    writeWindow(perm_ptr->permutation[0], perm_ptr->permutation[1], tr->symbol);
                                break;

                                //Acc and rej, See if there are transitions (q[2],_)->ACCEPT/REJECT
                                case ACCEPT:
                                    writeWindow(perm_ptr->permutation[0], perm_ptr->permutation[1], encodeStateId(acc_state_id));
                                break;
                                case REJECT:
                                    writeWindow(perm_ptr->permutation[0], perm_ptr->permutation[1], encodeStateId(rej_state_id));
                                break;
                                default:
                                break;
                            }
                        }
                    }
                }
            }   

            perm_ptr = perm_ptr->next;
        }
        
        list_dealloc_permutations(permutations);
    }

    static void calculate_permutations(permutation_node * l, tm_properties * prop){
        int cell_sym_len = prop->alphabet_length + prop->states_length;

        for(int i = 0; i < cell_sym_len; ++i){
            for(int j = 0; j < cell_sym_len; ++j){
                for(int k = 0; k < cell_sym_len; ++k){

                    /* First part excludes #s# s#s ##s s##
                     * Second part excludes duplicate state occurrences
                     * Third part excludes states next to a right delimiter like _q#
                     */
                    if(((i != 0 || k != 0) && (j != 0)) 
                        &&
                      ((isState(i) && !isState(j) && !isState(k)) || (!isState(i) && isState(j) && !isState(k)) || 
                       (!isState(i) && !isState(j) && isState(k)) || (!isState(i) && !isState(j) && !isState(k)))
                        &&
                      !(isState(j) && k == 0))
                    {
                        /* In windows state ids are encoded as negative numbers
                         * Alphabet symbols are actual chars and "states ids" are just indexes of the states array
                         * This is done for efficiency in calculate_legaL_windows() 
                         * because delta takes chars and actual state numbers as input, not ids
                         */
                        l->permutation[0] = (isState(i) ? encodeStateId(i-prop->alphabet_length) : prop->alphabet[i]); 
                        l->permutation[1] = (isState(j) ? encodeStateId(j-prop->alphabet_length) : prop->alphabet[j]);
                        l->permutation[2] = (isState(k) ? encodeStateId(k-prop->alphabet_length) : prop->alphabet[k]);
                        l = add_permutation(l);
                    }

                }   
            }
        }
    }
#endif//FORMULA
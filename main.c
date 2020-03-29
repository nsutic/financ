#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200112L
#include "callbacks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <time.h>
#include "llist.h"
#include "utility.h"
#include "file_ops.h"
#include "errors.h"
#define GETOPT_FMT "ha:ioc:d:"
#define DELIMITER ",\n"
#define DATE_FMT "dd/mm/yyyy"

// Function renames
#define ADD_CATEGORY(list,categ,size) push(list,categ,size)
#define FIND_CATEGORY(list,criteria,func) find_data(list,criteria,func)
#define FOR_CATEGORY(list,func) iterate_list(list,func)
#define SAME_STR(A,B) strcmp(A,B) == 0
#define NOT_SAME_STR(A,B) strcmp(A,B) != 0

#define BUDGET_FILE "budget.txt"
#define TRANSAC_FILE "history.txt"
#define HIDDEN_CATEG_FILE "hidden.txt"
#define DATA_DIR "/.config/financ/"

#define CREATE_FILE(name) if(0 != access(name,F_OK)) \
		fclose(fopen(name,"w"));
//// Globals
node *categories=NULL;
node *hidden_categories=NULL;
unsigned line_counter=0;
float global_sum=0;
int global_int=0;
int monthly_inflow=0;
int monthly_outflow=0;
float outflow=0;
int transac_counter=0;

#define OVERWRITE_YES (1)

void set_datemsk_env()
{
	char *datemsk_file = malloc(strlen(DATA_DIR"datemsk.txt")+strlen(getenv("HOME"))+1);
	strcpy(datemsk_file,getenv("HOME"));
	strcat(datemsk_file,DATA_DIR"datemsk.txt");
	FILE *file_handler = fopen(datemsk_file,"w");
	ASSERT(NULL != file_handler,"Error opening file");

	fprintf(file_handler,"%%d/%%m/%%Y");
	fclose(file_handler);

	ASSERT(0 == setenv("DATEMSK",datemsk_file,OVERWRITE_YES),"Error setting DATEMSK env. var.");
	free(datemsk_file);

}

char *current_date_string()
{
		time_t now=time(NULL);
		struct tm *broken_down_now=gmtime(&now);
		char *date_string=malloc(sizeof DATE_FMT);
		sprintf(date_string,"%d/%d/%d",broken_down_now->tm_mday,
	                                      broken_down_now->tm_mon+1,
	                                      broken_down_now->tm_year+1900);
		return date_string;
}

void print_category(const void *input)
{
		category *categ=(category*)input;
		global_int=0;
		FOR_CATEGORY(categories,&max_len_check);

		// Skip empty categories
		if(!categ->state && !categ->plan) {
			return;
		}

		if(categ->budgeted < categ->plan && categ->plan != 0) {
			printf("%12.2f\t%-*s\t\tUnbudgeted:%12.2f\n",
					categ->state,
					global_int,
					categ->name,
					categ->plan-categ->budgeted);
		}
		else {
			printf("%12.2f\t%-*s\t\n",
					categ->state,
					global_int,
					categ->name);
		}
}

int category_read_check(const void *data, const void *name)
{
		if(data == NULL) {
				return 0;
		}

		const category *data_ptr=data;
		const char *name_ptr=name;

		return SAME_STR(data_ptr->name,name_ptr);
}

category *new_category(const char *name_str)
{
		category *new=malloc(sizeof *new);
		new->name=malloc(strlen(name_str)+1);
		strcpy(new->name,name_str);
		new->state=0;
		new->budgeted=0;
		new->plan=0;
		return new;
}



int parse_budget_line(const char *_line)
{
		char *line = strdup(_line);

		char *categ_name=strtok(line,DELIMITER);
		float planned=strtof(strtok(NULL,DELIMITER),NULL);

		void *found_in_categories = FIND_CATEGORY(categories,categ_name,&category_read_check);
		void *found_in_hidden = FIND_CATEGORY(categories,categ_name,&category_read_check);

		if(found_in_categories == NULL &&
		   found_in_hidden == NULL) {
				category *new=new_category(categ_name);
				new->plan=planned;
				ADD_CATEGORY(&categories,new,sizeof *new);
		}
		
		else {
				ASSERT(0,"Category duplicate detected!");
		}

		free(categ_name);
		return 0;
}


int parse_transaction(const char *_line)
{
		char *line = strdup(_line);

		char *t_date_string;
		int io_flag;
		char *category_name;
		float amount;
		line_counter++;

		time_t curtime=time(NULL);
		struct tm *temp_tm=gmtime(&curtime);

		// FORMAT: date,[1/0/],category,amount,description

		t_date_string=strtok(line,DELIMITER);
		struct tm *t_date;
		t_date=getdate(t_date_string);

		ASSERT(NULL != t_date,"Getdate failed");

		io_flag=strtol(strtok(NULL,DELIMITER),NULL,10);
		category_name=strtok(NULL,DELIMITER);
		amount=strtof(strtok(NULL,DELIMITER),NULL);

		category *found_ptr=FIND_CATEGORY(categories,category_name,&category_read_check);

		if(found_ptr == NULL) {
				fprintf(stderr,"t: %d\n",line_counter);
				ASSERT(0,"Error in transaction. No category found!");
		}
		else {
				switch(io_flag) {
						case 0:
							found_ptr->state -= amount;
							break;
						case 2:
							if(NOT_SAME_STR(found_ptr->name,"TBB") &&
							   (t_date->tm_mon == temp_tm->tm_mon) &&
							   (t_date->tm_year == temp_tm->tm_year)) {
									found_ptr->budgeted -= amount;
							}
							found_ptr->state -= amount;
							break;
						case 12:
							if(NOT_SAME_STR(found_ptr->name,"TBB") &&
							  (t_date->tm_mon == temp_tm->tm_mon) &&
							  (t_date->tm_year == temp_tm->tm_year)) {
									found_ptr->budgeted += amount;
							}
							found_ptr->state += amount;
							break;
						case 1:
							found_ptr->state += amount;
							break;
				}
		}

		free(line);
		return 0;
}

char *create_transaction_line(const char *date_arg,const int io_flag,const char *categ_name,const float amount,const char *description)
{
		size_t buf_size=0;
		buf_size += strlen(date_arg);
		buf_size += strlen(categ_name);
		if(NULL != description) buf_size += strlen(description);
		// Extra padding for io_flag and more
		buf_size += 20;
		char *line=malloc(buf_size);

		ASSERT(NULL != line,"Malloc failed");

		if(description == NULL) {
				sprintf(line,"%s,%d,%s,%.2f\n",date_arg,io_flag,categ_name,amount);
		}
		else {
				sprintf(line,"%s,%d,%s,%.2f,%s\n",date_arg,io_flag,categ_name,amount,description);
		}
		return line;

}

void cover(category *source_ptr,float cover_with)
{
	node *iterator = categories;
	category *current_ptr;

	ASSERT(source_ptr->state >= cover_with,"Not enough currency in cover category!");

	while(iterator != NULL) {

		current_ptr = iterator->data;

		if(current_ptr == source_ptr || current_ptr->state >= 0) {
			iterator = iterator->next;
			continue;
		}

		if(source_ptr->state == 0 || cover_with == 0) {
			return;
		}

		float transac_amount = cover_with > abs(current_ptr->state) ? abs(current_ptr->state) : cover_with;

		parse_transaction(append_to_file(TRANSAC_FILE,create_transaction_line(current_date_string(),2,source_ptr->name,transac_amount,NULL)));
		parse_transaction(append_to_file(TRANSAC_FILE,create_transaction_line(current_date_string(),12,current_ptr->name,transac_amount,NULL)));

		source_ptr->state -= transac_amount;
		cover_with -= transac_amount;
		iterator = iterator->next;

	}
}

void quick_budget(category *source_ptr)
{
		node *iterator=categories;
		category *current_ptr;

		while(iterator != NULL) {

				current_ptr=(category*)(iterator->data);

				if((current_ptr == source_ptr) || (current_ptr->budgeted == current_ptr->plan)) {
						iterator=iterator->next;
						continue;
				}
				
				if(source_ptr->state == 0) {
						return;
				}
				
				float transac_amount;
				float unbudgeted = current_ptr->plan - current_ptr->budgeted;
				transac_amount = source_ptr->state > unbudgeted ? unbudgeted : source_ptr->state;

				parse_transaction(append_to_file(TRANSAC_FILE,create_transaction_line(current_date_string(),2,source_ptr->name,transac_amount,NULL)));
				parse_transaction(append_to_file(TRANSAC_FILE,create_transaction_line(current_date_string(),12,current_ptr->name,transac_amount,NULL)));

				source_ptr->state -= transac_amount;

				iterator=iterator->next;
		}
}

void goto_dir(const char *path)
{
		char *data_dir=malloc(strlen(getenv("HOME")) + strlen(DATA_DIR) + 1);
		strcpy(data_dir,getenv("HOME"));
		strcat(data_dir,DATA_DIR);

		if(0 != access(data_dir,F_OK)) {
			ASSERT(-1 != system("mkdir -p ~"DATA_DIR),"Error making "DATA_DIR);
		}

		ASSERT(-1 != chdir(data_dir),"Error changing directory");
		free(data_dir);
}

float test_func(char *line)
{
	puts(line);
}

int main(int argc, char **argv)
{
		goto_dir(DATA_DIR);

		// Set datemsk to %d/%m/%Y
		set_datemsk_env();

		CREATE_FILE(BUDGET_FILE);
		CREATE_FILE(TRANSAC_FILE);
		CREATE_FILE(HIDDEN_CATEG_FILE);

		char inflow_flag=0;
		char outflow_flag=0;
		char state_flag=0;
		char quick_flag=0;
		char cover_flag=0;
		char option;
		int long_option_index;

		parse_file(BUDGET_FILE,&parse_budget_line);
		parse_file(HIDDEN_CATEG_FILE,&parse_budget_line);

		line_counter=0;
		parse_file(TRANSAC_FILE,&parse_transaction);

		//range_query("/home/nikola/.config/financ/history.txt","1/2/2020","29/2/2020",test_func);

		struct option long_options[]= {
				{ "to", required_argument, 0, 0 },
				{ "from", required_argument, 0, 0 },
				{ "amount", required_argument, 0, 'a' },
				{ "category", required_argument, 0, 'c' },
				{ "budget-state", no_argument, 0, 0 },
				{ "date", required_argument, 0, 'd' },
				{ "description", required_argument, 0, 0},
				{ "quick-budget", no_argument, 0, 0},
				{ "cover", no_argument, 0, 0},
				{ 0, 0, 0, 0 }
		};

		char *to_arg=NULL;
		char *from_arg=NULL;
		char *amount_arg=NULL;
		char *category_arg=NULL;
		char *date_arg=NULL;
		char *description=NULL;

		while((option=getopt_long(argc,argv,GETOPT_FMT,long_options,&long_option_index)) != -1) {

				// Long option found
				if(option == 0) {
						switch(long_option_index) {
								case 0:
									to_arg=strdup(optarg);
									//to_arg=malloc(strlen(optarg)+1);
									//strcpy(to_arg,optarg);
									break;
								case 1:
									from_arg=strdup(optarg);
									//from_arg=malloc(strlen(optarg)+1);
									//strcpy(from_arg,optarg);
									break;
								case 4:
									state_flag=1;
									break;
								case 5:
									date_arg=strdup(optarg);
									break;
								case 6:
									description=strdup(optarg);
									break;
								case 7:
									quick_flag=1;
									break;
								case 8:
									cover_flag=1;
									break;
						}
				}

				switch(option) {
						case 'i':
							inflow_flag=1;
							break;
						case 'o':
							outflow_flag=1;
							break;
						case 'a':
							amount_arg=strdup(optarg);
							break;
						case 'c':
							category_arg=strdup(optarg);
							break;
						case 'd':
							date_arg=strdup(optarg);
							break;

							
				}
		}

		if(NULL == date_arg) {
				date_arg=current_date_string();
		}

		if(cover_flag && NULL != category_arg && NULL != amount_arg) {
			cover(FIND_CATEGORY(categories,category_arg,&category_read_check),strtof(amount_arg,NULL));
			free(category_arg);
			free(amount_arg);
		}
		else if(state_flag) {
				FOR_CATEGORY(categories,&print_category);
				
				global_sum=0;
				FOR_CATEGORY(categories,&sum_state);
				printf("Net worth: %.2f\n",global_sum);

				global_sum=0;
				FOR_CATEGORY(categories,&sum_budgeted);
				printf("Budgeted this month: %.2f\n",global_sum);

				global_sum=0;
				FOR_CATEGORY(categories,&sum_plan);
				printf("Budget size: %.2f\n",global_sum);
		}
		// Transfer from one category to another
		else if(to_arg != NULL && from_arg != NULL && amount_arg != NULL) {

				// Set transaction date to current date if none given


				if(FIND_CATEGORY(categories,from_arg,&category_read_check) == NULL &&
				   FIND_CATEGORY(hidden_categories,from_arg,&category_read_check) == NULL) {
						ASSERT(0,"--from category doesn't exist!");
				}
				
				if(FIND_CATEGORY(categories,to_arg,&category_read_check) == NULL &&
				   FIND_CATEGORY(hidden_categories,to_arg,&category_read_check) == NULL) {
						ASSERT(0,"--to category doesn't exist!");
				}

				append_to_file(TRANSAC_FILE,create_transaction_line(date_arg,2,from_arg,strtof(amount_arg,NULL),description));
				append_to_file(TRANSAC_FILE,create_transaction_line(date_arg,12,to_arg,strtof(amount_arg,NULL),description));
				free(to_arg);
				free(from_arg);
				free(amount_arg);
				free(date_arg);
		}
		// Make a new transaction with inflow/outflow
		else if(category_arg != NULL && amount_arg != NULL && (inflow_flag || outflow_flag)) {
				
				if(FIND_CATEGORY(categories,category_arg,&category_read_check) == NULL &&
				   FIND_CATEGORY(hidden_categories,category_arg,&category_read_check) == NULL) {
						ASSERT(0,"Category doesn't exist!");
				}

				append_to_file(TRANSAC_FILE,create_transaction_line(date_arg,1 == inflow_flag,category_arg,strtof(amount_arg,NULL),description));
				free(category_arg);
		}
		else if(quick_flag && category_arg != NULL) {
		   		category *source_ptr=FIND_CATEGORY(categories,category_arg,&category_read_check);

				if(source_ptr == NULL) {
						ASSERT(0,"Quick budget category source doesn't exist!");
				}
				
				quick_budget(source_ptr);
		}
		else {
				printf("Usage:\n"
				"   --budget-state - Get budget state\n"
				"   --quick-budget -c <name> - Quick budget transactions from category <name>\n"
				"   --from <category> - Set category for transfer outflow\n"
				"   --to <category> - Set category for transfer inflow\n"
				"   -a --amount <value> - Set the transaction amount\n"
				"   -d --date <dd/mm/yyyy> - Set the transaction date (optional)\n"
				"   -c --category <name> - Set the transaction category\n"
				"   --description <description> - Set the transaction description\n"
				"   --cover - Use category to cover overspending\n"
				"   -i - Set inflow\n"
				"   -o - Set outflow\n");
		}

		//free(category_arg);
		if(NULL != categories) free_list(categories);
		if(NULL != hidden_categories) free_list(hidden_categories);
		if(NULL != description) free(description);
		return 0;
}

#include <iostream>
#include <random>
#include <algorithm>
#include <unordered_map>
#include <vector>

#define TOT_PAGE_NUM 3 // total page number in DB
#define MEMORY_CAP 20 // memory capacity
#define ELEMENTS_NUM_IN_PAGE 10 // page capacity 

/*
  simple modelling of external merge sort in DBMS
*/

std::unordered_map<std::string, std::vector<int>> pages;
std::unordered_map<std::string, std::vector<int>> copied_pages;

int fetched_page_ptr;
std::vector<int> *in_memory;
int *data_offsets;
int output_buffer_cap;
int output_count = 0;
std::string get_page_name(std::string name, int i){
  return name+std::to_string(i);
} 

// these three functions are for generating random keys and put it into virtual pages in database
int get_random (int min, int max){
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_int_distribution<int> dist(min, max);
  return dist(mt);
}

void generate_rand_numbers(std::vector<int>& a, int max_val, int until) {
  for(int i = 0 ; i < until; i++) {
    int rand = get_random (1, max_val);
    a.push_back(rand);
  }
}
void make_rand_pages() {
  std::vector<int> elements;
  for(int i = 0; i < TOT_PAGE_NUM; i++){
    elements.clear();
    generate_rand_numbers(elements, TOT_PAGE_NUM*ELEMENTS_NUM_IN_PAGE, ELEMENTS_NUM_IN_PAGE);
    pages[get_page_name("page", i)] = elements;
  }
}
//fetch the page, first pass : data_offset would be null
void fetch_page(int num, int data_offset[] = nullptr, int in_memory_elements = MEMORY_CAP/ (TOT_PAGE_NUM)){
  int j = 0;
  for(int i = fetched_page_ptr ; i < fetched_page_ptr + num ; i++){
    if(i > TOT_PAGE_NUM-1) {
      fetched_page_ptr += i - fetched_page_ptr;
      return;
    }
    std::string pagename = get_page_name("page", i);
    // for first pass : get entire page
    if(data_offset == NULL){
      //page number is from disk so it could grow large ,but in_memory
      // has finite page number : separate variable j
      in_memory[j] = pages[pagename]; 
    }
    // for second,third,fourth ..
    else {
      // check if it could reach the end in this pass
      int till;
      int isFull = pages[pagename].size() <= data_offset[i] + in_memory_elements ? 1 : 0;
      if(isFull) {
        till = pages[pagename].size();
      }
      else {
        till =  data_offset[i] + in_memory_elements;
      }
      for(int k = data_offset[i]; k < till; k++){
          in_memory[i].push_back(pages[pagename][k]); 
      }
      if(isFull) {
        data_offset[i] = pages[pagename].size();
        return;
      }
      else data_offset[i] += in_memory_elements;
    }
    j++;
  }   
  //used for fetching first pages into memory : keeping track of would-be-fetched page number
  fetched_page_ptr += num;
}

std::vector <int> merge_sort(int num_cursor, int data_offsets[], int in_memory_elements){
  int *cursor = new int[num_cursor];
  std::vector <int> &output = in_memory[num_cursor]; // in_memory[19] => output 자리
  std::string pagename = get_page_name("output", output_count);
  for(int i = 0 ; i < num_cursor; i++){
    cursor[i] = 0;
  }

  while(1) {
    for(int i = 0 ; i < num_cursor; i++){
      // if cursor reached end
      if(cursor[i] >= in_memory[i].size()){
        fetched_page_ptr = i;
        in_memory[i].clear();
        std::string name = get_page_name("page",i);
        if(data_offsets[i] >= pages[name].size()){
          cursor[i] = -1;
        }
        else {
          fetch_page(1, data_offsets, in_memory_elements);
          cursor[i] = 0;
        }
      }
    }
    int min = -1;
    int min_num = 0;
    // initialize min
    for(int i = 0 ; i < num_cursor; i++){
      if(cursor[i] != -1) {
        min = in_memory[i][cursor[i]];
        min_num = i;

        break;
      }
    }
    // 모든 커서가 끝에 도달
    if(min == -1) {
      //before terminating
      //write out leftover output
      for(int i = 0 ; i < output.size(); i++){
        // pagename = get_page_name("output", output_count);
        if(copied_pages[pagename].size() >= ELEMENTS_NUM_IN_PAGE){
          output_count+=1;
          pagename = get_page_name("output", output_count);
        }
        copied_pages[pagename].push_back(output[i]);
      }
      output.clear();

      //terminate
      delete [] cursor;
      return output;
    }
   
    for(int i = 0 ; i < num_cursor; i++){
      if(cursor[i] == -1) continue;
      if(min > in_memory[i][cursor[i]]){
        min_num = i;
        min = in_memory[i][cursor[i]];
      }
    }
    cursor[min_num] += 1;
    output.push_back(min);

    //if output buffer exceeds its given capacity
    if(output.size() >= output_buffer_cap){
      for(int i = 0 ; i < output.size(); i++){
        // write out buffer to disk : if that page is full, make new page 
        if(copied_pages[pagename].size() >= ELEMENTS_NUM_IN_PAGE){
          output_count+=1;
          pagename = get_page_name("output", output_count);
        }
        copied_pages[pagename].push_back(output[i]);
      }
      output.clear();
    } 
  }


  delete [] cursor;
}

void init_merge(){
  // for pass 0 (first pass) : 
  // memory can accommodate n pages from disk
  int n = TOT_PAGE_NUM*ELEMENTS_NUM_IN_PAGE / MEMORY_CAP;
  if( n < 1) {
    printf("\nYou can sort it using quick sort, memory can fit your database \n");
    return;
  }
  in_memory = new std::vector<int> [n];
  //pass 0 
  fetched_page_ptr = 0; // keeps track of would-be-fetched page number
  while(fetched_page_ptr < TOT_PAGE_NUM){
    int s = fetched_page_ptr;
    fetch_page(n);
    for(int i = 0 ; i < n; i++) 
      sort(in_memory[i].begin(), in_memory[i].end());
  // back to disk
    int j = 0;
    for(int i = s; i < fetched_page_ptr; i++){
      pages[get_page_name("page", i)] = in_memory[j];
      in_memory[j].clear();
      j++;
    }
  }

  fetched_page_ptr = 0;
  // here, n is for n_way merge sort =>n is also total number of pages in data you want to sort
  n = TOT_PAGE_NUM;
  // each of page inside of memory could keep their elements up to in_memory_elements
  int in_memory_elements = MEMORY_CAP / (n+1);
  // leftover capacity would be added to output buffer capacity
  output_buffer_cap = in_memory_elements + MEMORY_CAP % (n+1);
  // page_number + output => n + 1
  in_memory = new std::vector<int>[n+1];
  
  // save each of pages' inside data offsets
  data_offsets = new int[n];
  std::fill_n(data_offsets, n, 0);

  // fetch first part of pages 
  fetch_page(n, data_offsets, in_memory_elements);
  merge_sort(n, data_offsets, in_memory_elements);

  delete [] in_memory;
  delete [] data_offsets;
}

int main() {
  // if you have custom key sets, then comment make_rand_pages out and save that key sets to pages 
  make_rand_pages();

  // before sorting
  for(int j = 0 ; j < TOT_PAGE_NUM; j ++ ){
  std::cout << '\n';
  std::string pagen = get_page_name("page", j);

  for(int i = 0 ; i < pages[pagen].size(); i++){
    std::cout << pages[pagen][i] << ' ';
  }
  }

  init_merge();
  std::cout << '\n';
  //oufter sorting
  for(int j = 0 ; j <= output_count; j++){
  std::cout << '\n';
  std::string pagename = get_page_name("output", j);
  for(int i = 0 ; i < copied_pages[pagename].size(); i++){
      std::cout << copied_pages[pagename][i] << ' ';
  }
  }
}
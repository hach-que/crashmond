#include "crashmond.h"
#include "curl/curl.h"

static size_t read_callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
  int fd = *((int*)userdata);
  return read(fd, ptr, size * nmemb);
}

static size_t write_callback(void* ptr, size_t size, size_t nmemb) {
  char* data = malloc((size * nmemb) + 1);
  memset(data, 0, (size * nmemb) + 1);
  memcpy(data, ptr, size * nmemb);
  sd_journal_print(LOG_NOTICE, "%s", data);
  free(data);
  return size * nmemb;
}

int submit_crash_report(char* url, int fd) {
  CURL* curl;
  int result;
  
  curl = curl_easy_init();
  if (!curl) {
    sd_journal_print(LOG_WARNING, "failed to initialize CURL");
    return -1;
  }
  
  int* fd_store = malloc(sizeof(int));
  *fd_store = fd;
  
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, fd_store);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  
  result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    sd_journal_print(LOG_WARNING, "failed to submit crash report: %s", curl_easy_strerror(result));
  }
  
  free(fd_store);
  curl_slist_free_all(chunk);
  curl_easy_cleanup(curl);
  
  return (result == CURLE_OK) ? 0 : -1;
}

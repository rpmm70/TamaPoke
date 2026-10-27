// Test runner: runs every registered TEST (optionally filtered) and prints a summary.

#pragma region Includes
#include "framework.h"
#include "shim/Arduino.h"
#include <cstdio>
#include <cstring>
#pragma endregion

#pragma region Registry and failure collection
std::vector<TestCase> &testRegistry() {
  static std::vector<TestCase> reg;
  return reg;
}

int gAssertCount = 0;
static std::vector<std::string> gFailures;

void testFail(const char *file, int line, const std::string &msg) {
  const char *base = strrchr(file, '/');
  std::ostringstream os;
  os << (base ? base + 1 : file) << ":" << line << ": " << msg;
  gFailures.push_back(os.str());
}
#pragma endregion

#pragma region Entry point
int main(int argc, char **argv) {
  const char *filter = argc > 1 ? argv[1] : 0;
  mockSerialSilence(true);

  int passed = 0, failed = 0, skipped = 0, xfail = 0, xpass = 0;
  std::string lastSuite;
  std::vector<std::string> failedNames;
  std::vector<std::string> knownIssues;
  std::vector<std::string> fixedIssues;

  for (size_t i = 0; i < testRegistry().size(); i++) {
    TestCase &tc = testRegistry()[i];
    std::string full = tc.suite + "." + tc.name;
    if (filter && full.find(filter) == std::string::npos) { skipped++; continue; }
    if (tc.suite != lastSuite) {
      printf("\n\033[1m%s\033[0m\n", tc.suite.c_str());
      lastSuite = tc.suite;
    }
    gFailures.clear();
    // clean state for each test
    mockSetMillis(0);
    mockClearForcedRandom();
    randomSeed(0xC0FFEE);
    tc.fn();
    if (tc.knownIssue) {
      if (gFailures.empty()) {
        printf("  \033[33mXPASS\033[0m %s  (no longer fails: remove TEST_KNOWN_ISSUE)\n", tc.name.c_str());
        xpass++;
        fixedIssues.push_back(full);
      } else {
        printf("  \033[33mKNOWN\033[0m %s  -- %s\n", tc.name.c_str(), tc.note.c_str());
        for (size_t j = 0; j < gFailures.size(); j++)
          printf("        %s\n", gFailures[j].c_str());
        xfail++;
        knownIssues.push_back(full + ": " + tc.note);
      }
      continue;
    }
    if (gFailures.empty()) {
      printf("  \033[32mok\033[0m   %s\n", tc.name.c_str());
      passed++;
    } else {
      printf("  \033[31mFAIL\033[0m %s\n", tc.name.c_str());
      for (size_t j = 0; j < gFailures.size(); j++)
        printf("        %s\n", gFailures[j].c_str());
      failed++;
      failedNames.push_back(full);
    }
  }

  printf("\n----------------------------------------\n");
  printf("%d tests, %d asserts, %d passed, %d failed", passed + failed + xfail + xpass,
         gAssertCount, passed, failed);
  if (xfail) printf(", %d known bugs", xfail);
  if (xpass) printf(", %d bugs already fixed", xpass);
  if (skipped) printf(", %d filtered out", skipped);
  printf("\n");
  if (failed) {
    printf("\nFailures:\n");
    for (size_t i = 0; i < failedNames.size(); i++) printf("  - %s\n", failedNames[i].c_str());
  }
  if (xfail) {
    printf("\nKnown bugs (documented, they do not break the suite):\n");
    for (size_t i = 0; i < knownIssues.size(); i++) printf("  - %s\n", knownIssues[i].c_str());
  }
  if (xpass) {
    printf("\nKnown bugs that NO longer fail (remove TEST_KNOWN_ISSUE):\n");
    for (size_t i = 0; i < fixedIssues.size(); i++) printf("  - %s\n", fixedIssues[i].c_str());
  }
  return failed ? 1 : 0;
}
#pragma endregion

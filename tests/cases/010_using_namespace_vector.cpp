#include <iostream>
#include <vector>

using namespace std;

int main() {
  vector<int> xs;
  xs.push_back(8);
  xs.push_back(13);
  cout << "size=" << xs.size() << endl;
  cout << "last=" << xs[1] << endl;
  return xs[0] + xs[1];
}

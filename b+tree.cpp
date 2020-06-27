
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <queue>
#include <stack>
using namespace std;



struct Index_Entry {
	int key, bid;

	Index_Entry() : key(0), bid(0) {}
	Index_Entry(int k, int b) : key(k), bid(b) {}

};

struct Data_Entry {
	int key, value;

	Data_Entry() : key(0), value(0) {}
	Data_Entry(int k, int v) : key(k), value(v) {}

};

class B_Tree {
public:
	int block_size=0, rootBID=0, depth=0, lastBID=0, num_entry=0;
	int leaf_lower_bound=0, leaf_upper_bound=0, non_leaf_split_lower=0, non_leaf_split_upper=0;

	const int zeroBID = 0;

	char* filename;
	fstream fs;

	queue<pair<int, int>> print_q;

	B_Tree(char* fname);
	~B_Tree();

	void Update_Header();
	void Insert(int key, int val);
	void SearchByKey(int key, char* fname);
	void SearchByRange(int start, int end, char* fname);
	void Print(char* fname);
};

B_Tree::B_Tree(char* fname) {
	filename = fname;
	fs.open(filename, ios::in | ios::out | ios::binary);

	if (fs.fail()) {
		return;
	}

	fs.read(reinterpret_cast<char*>(&block_size), sizeof(block_size));
	fs.read(reinterpret_cast<char*>(&rootBID), sizeof(rootBID));
	fs.read(reinterpret_cast<char*>(&depth), sizeof(depth));
	fs.seekg(0, ios::end);

	lastBID = fs.tellg();
	lastBID = (lastBID - 12) / block_size;
	num_entry = (block_size - 4) / 8;

	if (num_entry % 2 == 1) {
		leaf_lower_bound = (num_entry - 1) / 2;
		non_leaf_split_lower = (num_entry + 1) / 2;
	}
	else {
		leaf_lower_bound = (num_entry - 1) / 2 + 1;
		non_leaf_split_lower = (num_entry) / 2;
	}

	leaf_upper_bound = num_entry - 1;
	non_leaf_split_upper = num_entry - non_leaf_split_lower;
}

B_Tree::~B_Tree() {
	fs.close();
}

void B_Tree::Update_Header() {
	fs.seekp(4, ios::beg);
	fs.write(reinterpret_cast<const char*>(&rootBID), sizeof(rootBID));
	fs.write(reinterpret_cast<const char*>(&depth), sizeof(depth));
}

void B_Tree::Insert(int key, int val) {

	if (rootBID == 0) {
		vector<Data_Entry> node(num_entry);
		node[0].key = key;
		node[0].value = val;

		rootBID = 1;
		lastBID += 1;

		fs.seekp(0, ios::end);
		for (int i = 0; i < num_entry; i++) {
			fs.write(reinterpret_cast<const char*>(&node[i]), sizeof(Data_Entry));
		}

		fs.write(reinterpret_cast<const char*>(&zeroBID), sizeof(zeroBID));
	}
	else {
		int nowoffset = 12 + ((rootBID - 1) * block_size);
		int nowBID = rootBID, newBID = 0, parentBID = 0;
		int nowdepth = 0;
		int leftBID, newkey;

		Data_Entry tmp_data_entry;
		Index_Entry tmp_index_entry;

		stack<int> parent_bid_stack;


		parent_bid_stack.push(0);

		while (nowdepth != depth) {

			fs.seekg(nowoffset, ios::beg);
			fs.read(reinterpret_cast<char*>(&leftBID), sizeof(leftBID));
			for (int i = 0; i < num_entry; i++) {
				fs.read(reinterpret_cast<char*>(&tmp_index_entry), sizeof(Index_Entry));
				if (tmp_index_entry.key <= key && tmp_index_entry.key != 0) {
					leftBID = tmp_index_entry.bid;
				}
				else {
					break;
				}
			}
			parent_bid_stack.push(nowBID);
			nowBID = leftBID;
			nowoffset = 12 + ((nowBID - 1) * block_size);
			nowdepth += 1;
		}

		if (nowdepth == depth) {
			vector<Data_Entry> leaf_node;

			int entrycnt = 0;
			bool insert_check = false;

			fs.seekg(nowoffset, ios::beg);

			for (int i = 0; i < num_entry; i++) {
				fs.read(reinterpret_cast<char*> (&tmp_data_entry), sizeof(Data_Entry));
				if (tmp_data_entry.key != 0) {
					entrycnt += 1;
				}
				if (tmp_data_entry.key > key || tmp_data_entry.key == 0) {
					if (!insert_check) {
						leaf_node.push_back(Data_Entry(key, val));
						insert_check = true;
						entrycnt += 1;
					}
					leaf_node.push_back(tmp_data_entry);
					if (entrycnt == num_entry)
						break;

				}
				else if (tmp_data_entry.key < key)
					leaf_node.push_back(tmp_data_entry);

			}

			if (entrycnt == num_entry) {
				vector<Data_Entry> left_node(num_entry);
				vector<Data_Entry> right_node(num_entry);

				for (int i = 0; i < leaf_lower_bound; i++) {
					left_node[i] = leaf_node[i];
				}
				for (int i = leaf_lower_bound; i <= leaf_upper_bound; i++) {
					right_node[i - leaf_lower_bound] = leaf_node[i];
				}

				int tmpBID;
				int offset = nowoffset + block_size - 4;
				fs.seekp(offset, ios::beg);
				fs.read(reinterpret_cast<char*>(&tmpBID), sizeof(tmpBID));

				lastBID++;
				newBID = lastBID;

				fs.seekp(nowoffset, ios::beg);
				for (int i = 0; i < num_entry; i++) {
					fs.write(reinterpret_cast<const char*>(&left_node[i]), sizeof(Data_Entry));
				}
				fs.write(reinterpret_cast<const char*>(&newBID), sizeof(newBID));

				fs.seekp(0, ios::end);

				for (int i = 0; i < num_entry; i++) {
					fs.write(reinterpret_cast<const char*>(&right_node[i]), sizeof(Data_Entry));
				}
				fs.write(reinterpret_cast<const char*>(&tmpBID), sizeof(tmpBID));

				newkey = right_node[0].key;
				bool parent_check = true;

				while (parent_check) {
					parent_check = false;

					parentBID = parent_bid_stack.top();
					parent_bid_stack.pop();

					if (parentBID == 0) {
						vector<Index_Entry> index_node(num_entry);
						index_node[0].key = newkey;
						index_node[0].bid = newBID;

						depth += 1;
						lastBID += 1;
						newBID = lastBID;
						rootBID = newBID;

						fs.seekp(0, ios::end);
						fs.write(reinterpret_cast<const char*>(&nowBID), sizeof(nowBID));
						for (int i = 0; i < num_entry; i++) {
							fs.write(reinterpret_cast<const char*>(&index_node[i]), sizeof(Index_Entry));
						}
					}
					else {
						vector<Index_Entry> index_node;
						nowoffset = 12 + ((parentBID - 1) * block_size);
						entrycnt = 0;
						insert_check = false;
						fs.seekg(nowoffset, ios::beg);
						fs.read(reinterpret_cast<char*>(&leftBID), sizeof(leftBID));
						for (int i = 0; i < num_entry; i++) {
							fs.read(reinterpret_cast<char*> (&tmp_index_entry), sizeof(Index_Entry));
							if (tmp_index_entry.key > key) {
								if (!insert_check) {
									index_node.push_back(Index_Entry(newkey, newBID));
									insert_check = true;
								
								}
								entrycnt += 1;
							}
							else if (tmp_index_entry.key == 0) {
								if (!insert_check) {
									index_node.push_back(Index_Entry(newkey, newBID));
									insert_check = true;
								}
							}
							else if (tmp_index_entry.key < key)
								entrycnt += 1;
							index_node.push_back(tmp_index_entry);
						}

						if (entrycnt == num_entry) {
							if (!insert_check) {
								index_node.push_back(Index_Entry(newkey, newBID));
							}
							parent_check = true;

							vector<Index_Entry> index_left_node(num_entry);
							vector<Index_Entry> index_right_node(num_entry);

							for (int i = 0; i < non_leaf_split_lower; i++) {
								index_left_node[i] = index_node[i];
							}
							for (int i = 0; i < non_leaf_split_upper; i++) {
								index_right_node[i] = index_node[non_leaf_split_lower + i + 1];
							}

							int tmpBID;
							newkey = index_node[non_leaf_split_lower].key;
							tmpBID = index_node[non_leaf_split_lower].bid;

							lastBID += 1, newBID = lastBID, nowBID = parentBID;

							fs.seekp(nowoffset, ios::beg);
							fs.write(reinterpret_cast<const char*>(&leftBID), sizeof(leftBID));

							for (int i = 0; i < num_entry; i++) {
								fs.write(reinterpret_cast<const char*>(&index_left_node[i]), sizeof(Index_Entry));
							}

							fs.seekp(0, ios::end);
							fs.write(reinterpret_cast<const char*>(&tmpBID), sizeof(tmpBID));

							for (int i = 0; i < num_entry; i++) {
								fs.write(reinterpret_cast<const char*>(&index_right_node[i]), sizeof(Index_Entry));
							}
						}

						if (!parent_check) {
							int offset = nowoffset + 4;
							fs.seekp(offset, ios::beg);
							for (int i = 0; i < num_entry; i++) {
								fs.write(reinterpret_cast<const char*>(&index_node[i]), sizeof(Index_Entry));
							}
						}

					}
				}
			}
			else {
				fs.seekp(nowoffset, ios::beg);
				for (int i = 0; i < num_entry; i++) {
					fs.write(reinterpret_cast<const char*>(&leaf_node[i]), sizeof(Data_Entry));
				}
			}
		}
	}
	Update_Header();
}

void B_Tree::SearchByKey(int key, char* fname) {
	int nowoffset = 12 + ((rootBID - 1) * block_size);
	int nowBID = rootBID, nowdepth = 0, leftBID, value = 0;

	Data_Entry tmp_data_entry;
	Index_Entry tmp_index_entry;

	while (nowdepth != depth) {
		vector<Index_Entry> index_node(num_entry);

		fs.seekg(nowoffset, ios::beg);
		fs.read(reinterpret_cast<char*>(&leftBID), sizeof(leftBID));
		for (int i = 0; i < num_entry; i++) {
			fs.read(reinterpret_cast<char*>(&tmp_index_entry), sizeof(Index_Entry));
			if (tmp_index_entry.key <= key && tmp_index_entry.key != 0) {
				leftBID = tmp_index_entry.bid;
			}
			else {
				break;
			}
		}
		nowBID = leftBID;
		nowoffset = 12 + ((nowBID - 1) * block_size);
		nowdepth++;
	}
	fs.seekg(nowoffset, ios::beg);
	for (int i = 0; i < num_entry; i++) {
		fs.read(reinterpret_cast<char*>(&tmp_data_entry), sizeof(Data_Entry));
		if (tmp_data_entry.key == key) {
			value = tmp_data_entry.value;
			break;
		}
	}

	ofstream writetxt;
	writetxt.open(fname, ios::app);
	writetxt << key << "," << value << "\n";
	writetxt.close();
}

void B_Tree::SearchByRange(int start,int end, char* fname) {
	int nowoffset = 12 + ((rootBID - 1) * block_size);
	int nowBID = rootBID, nowdepth = 0, leftBID;

	Data_Entry tmp_data_entry;
	Index_Entry tmp_index_entry;

	while (nowdepth != depth) {
		vector<Index_Entry> index_node(num_entry);

		fs.seekg(nowoffset, ios::beg);
		fs.read(reinterpret_cast<char*>(&leftBID), sizeof(leftBID));
		for (int i = 0; i < num_entry; i++) {
			fs.read(reinterpret_cast<char*>(&tmp_index_entry), sizeof(Index_Entry));
			if (tmp_index_entry.key <= start && tmp_index_entry.key != 0) {
				leftBID = tmp_index_entry.bid;
			}
			else {
				break;
			}
		}
		nowBID = leftBID;
		nowoffset = 12 + ((nowBID - 1) * block_size);
		nowdepth++;
	}

	ofstream writetxt;
	writetxt.open(fname, ios::app);
	
	bool range_check = false;

	while (1) {
		fs.seekg(nowoffset, ios::beg);
		for (int i = 0; i < num_entry; i++) {
			fs.read(reinterpret_cast<char*>(&tmp_data_entry), sizeof(Data_Entry));
			if (tmp_data_entry.key == 0) {
				break;
			}
			else if (tmp_data_entry.key <= end) {
				if (tmp_data_entry.key >= start) {
					writetxt << tmp_data_entry.key << "," << tmp_data_entry.value << "\t";
				}
			}
			else {
				range_check = true;
				break;
			}
		}

		if (range_check) {
			break;
		}
		else {
			fs.seekg(nowoffset + block_size - 4, ios::beg);
			fs.read(reinterpret_cast<char*>(&nowBID), sizeof(nowBID));
			if (nowBID == 0) {
				break;
			}
			nowoffset = 12 + ((nowBID - 1) * block_size);
		}
	}

	writetxt << "\n";
	writetxt.close();
}

void B_Tree::Print(char* fname) {

	int nowoffset, nowBID, nowdepth, printdepth = 0, leftBID;
	Index_Entry tmp_index_entry;
	print_q.push({ rootBID, 0 });

	ofstream writetxt;
	writetxt.open(fname, ios::app);
	while (!print_q.empty()) {
		nowBID = print_q.front().first;
		nowdepth = print_q.front().second;
		nowoffset = 12 + ((nowBID - 1) * block_size);
		print_q.pop();

		if (printdepth == nowdepth) {
			writetxt << "<" << printdepth << ">\n";
			printdepth += 1;
		}

		fs.seekg(nowoffset, ios::beg);
		if (nowdepth != depth) {
			fs.read(reinterpret_cast<char*>(&leftBID), sizeof(leftBID));
			if (nowdepth == 0 && depth != 0) {
				print_q.push({ leftBID, nowdepth + 1 });
			}
		}
		for (int i = 0; i < num_entry; i++) {
			fs.read(reinterpret_cast<char*>(&tmp_index_entry), sizeof(Index_Entry));
			if (tmp_index_entry.bid == 0) {
				if (print_q.empty() || print_q.front().second == nowdepth + 1) {
					writetxt << "\n";
				}
				else {
					writetxt << ",";
				}
				break;
			}
			if (i != 0) {
				writetxt << ",";
			}
			writetxt << tmp_index_entry.key;

			if (nowdepth == 0 && depth != 0) {
				print_q.push({ tmp_index_entry.bid, nowdepth + 1 });
			}
		}
	}

	writetxt.close();
}

int main(int argc, char* argv[]) {

	ofstream createout;
	ifstream readtxt;
	char command = argv[1][0];
	char* binname = argv[2];
	char* readtxtfile, * outputtxtname;
	char linebuffer[22];

	B_Tree* BT = new B_Tree(binname);

	switch (command) {
	case 'c':
		int fileheader[3];
		createout.open(binname, ios::binary);

		fileheader[0] = atoi(argv[3]);
		fileheader[1] = 0;
		fileheader[2] = 0;
		createout.write(reinterpret_cast<const char*>(&fileheader), sizeof(fileheader));
		createout.close();
		cout << fileheader[0] << endl;
		break;
	case 'i':
		readtxtfile = argv[3];
		readtxt.open(readtxtfile);

		if (!readtxt.fail()) {
			while (readtxt.getline(linebuffer, sizeof(linebuffer))) {
				char* context;
				char* inserttmp = strtok_s(linebuffer, ",",&context);
				int key = atoi(inserttmp);
				inserttmp = strtok_s(NULL, " ", &context);
				int val = atoi(inserttmp);
				BT->Insert(key,val);
			}
		}
		

		break;
	case 's':
		readtxtfile = argv[3];
		outputtxtname = argv[4];
		readtxt.open(readtxtfile);

		if (!readtxt.fail()) {
			while (readtxt.getline(linebuffer, sizeof(linebuffer))) {
				int key = atoi(linebuffer);
				if (key == 0) {
					continue;
				}

				BT->SearchByKey(key, outputtxtname);
			}
		}

		break;
	case 'r':
		readtxtfile = argv[3];
		outputtxtname = argv[4];
		readtxt.open(readtxtfile);

		if (!readtxt.fail()) {
			while (readtxt.getline(linebuffer, sizeof(linebuffer))) {
				char* context;
				char* inserttmp = strtok_s(linebuffer, ",", &context);
				int start = atoi(inserttmp);
				inserttmp = strtok_s(NULL, " ", &context);
				int end = atoi(inserttmp);

				BT->SearchByRange(start,end, outputtxtname);
			}
		}

		break;
	case 'p':
		outputtxtname = argv[3];
		BT->Print(outputtxtname);

		break;
	}

	delete BT;
	return 0;
}
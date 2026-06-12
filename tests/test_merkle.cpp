#include "../core/merkle/merkle.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_merkle() {
    cur = "merkle";
    CHECK(hex(sha256(std::string("abc"))) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(hex(sha256(std::string(""))) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    Digest d = sha256(std::string("hello"));
    CHECK(from_hex(hex(d)) == d);

    std::vector<Digest> leaves;
    for (int i = 0; i < 9; i++) leaves.push_back(leaf_hash("item" + std::to_string(i)));
    Merkle tree(leaves);
    CHECK(tree.size() == 9);
    for (size_t i = 0; i < leaves.size(); i++) {
        auto pr = tree.proof(i);
        CHECK(Merkle::verify(leaves[i], pr, tree.root()));
    }
    auto pr0 = tree.proof(0);
    Digest bad = leaf_hash("tampered");
    CHECK(!Merkle::verify(bad, pr0, tree.root()));

    std::vector<Digest> two = {leaf_hash("a"), leaf_hash("b")};
    Merkle t2(two);
    CHECK(t2.root() == node_hash(two[0], two[1]));

    AuditChain chain;
    Digest h1 = chain.append(sha256(std::string("v1")));
    Digest h2 = chain.append(sha256(std::string("v2")));
    chain.append(sha256(std::string("v3")));
    CHECK(chain.verify());
    CHECK(chain.entries().size() == 3);
    CHECK(chain.head() != h1 && h1 != h2);
    CHECK(chain.entries()[1].prev == chain.entries()[0].hash);

    AuditChain c2;
    c2.append(sha256(std::string("x")));
    c2.append(sha256(std::string("y")));
    CHECK(c2.verify());
}

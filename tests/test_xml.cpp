#include "../extractor/xml/xml.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_xml() {
    cur = "xml";
    Xmlp r = parse_xml("<root a=\"1\" b=\"two\"><kid>hi</kid><kid>bye</kid></root>");
    CHECK(r != nullptr);
    CHECK(local_name(r->tag) == "root");
    CHECK(r->attr("a") == "1");
    CHECK(r->attr("b") == "two");
    CHECK(r->all("kid").size() == 2);
    CHECK(r->first("kid")->text == "hi");

    Xmlp ns = parse_xml("<x:root xmlns:x=\"u\"><x:item v=\"5\"/></x:root>");
    CHECK(local_name(ns->tag) == "root");
    CHECK(ns->first("item")->attr("v") == "5");

    Xmlp ent = parse_xml("<t>a &lt; b &amp; c &gt; d</t>");
    CHECK(ent->text == "a < b & c > d");

    Xmlp decl = parse_xml("<?xml version=\"1.0\"?>\n<!-- comment --><doc>x</doc>");
    CHECK(decl && local_name(decl->tag) == "doc" && decl->text == "x");

    Xmlp nest = parse_xml("<a><b><c>deep</c></b></a>");
    std::vector<Xmlp> cs;
    nest->find_all("c", cs);
    CHECK(cs.size() == 1 && cs[0]->text == "deep");

    Xmlp self = parse_xml("<a><e x=\"1\"/><e x=\"2\"/></a>");
    CHECK(self->all("e").size() == 2);
    CHECK(self->all("e")[1]->attr("x") == "2");
}

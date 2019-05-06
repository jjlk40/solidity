contract C {
    string public shortString = "any";
    string public longString = "anyanyanyanyanyanyanyanyanyanyanyanyanyanyanyany";
}
// ====
// EVMVersion: >=constantinople
// ----
// shortString() -> 0x20, 3, "any"

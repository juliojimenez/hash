f() {
  local x=1
  return $x
}
f; g() ( exit 2 ); g

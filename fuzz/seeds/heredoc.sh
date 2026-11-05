cat <<EOF
hello $USER $(id -u) $((1+2))
EOF
cat <<-"EOT"
	literal
	EOT

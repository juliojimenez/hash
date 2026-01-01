#!/usr/local/bin/hash-shell
#
# Example script demonstrating hash shell scripting features
#

echo "=== Hash Shell Script Demo ==="
echo ""

# ============================================================================
# Variables and arguments
# ============================================================================
echo "1. Variables and Arguments"
echo "   Script name: $0"
echo "   Argument count: $#"
if [ $# -gt 0 ]; then
    echo "   First argument: $1"
    echo "   All arguments: $@"
fi
echo ""

# ============================================================================
# If/then/else
# ============================================================================
echo "2. If/Then/Else"
count=5
if [ $count -lt 3 ]; then
    echo "   Count is less than 3"
elif [ $count -lt 7 ]; then
    echo "   Count is between 3 and 6"
else
    echo "   Count is 7 or more"
fi
echo ""

# ============================================================================
# For loop
# ============================================================================
echo "3. For Loop"
echo -n "   Counting: "
for i in 1 2 3 4 5; do
    echo -n "$i "
done
echo ""
echo ""

# ============================================================================
# While loop
# ============================================================================
echo "4. While Loop"
echo -n "   Countdown: "
n=5
while [ $n -gt 0 ]; do
    echo -n "$n "
    n=$((n - 1))
done
echo "Liftoff!"
echo ""

# ============================================================================
# File tests
# ============================================================================
echo "5. File Tests"
if [ -f "/etc/passwd" ]; then
    echo "   /etc/passwd exists and is a regular file"
fi
if [ -d "/tmp" ]; then
    echo "   /tmp exists and is a directory"
fi
if [ ! -e "/nonexistent" ]; then
    echo "   /nonexistent does not exist"
fi
echo ""

# ============================================================================
# String tests
# ============================================================================
echo "6. String Tests"
str1="hello"
str2="world"
if [ "$str1" = "hello" ]; then
    echo "   str1 equals 'hello'"
fi
if [ "$str1" != "$str2" ]; then
    echo "   str1 and str2 are different"
fi
if [ -n "$str1" ]; then
    echo "   str1 is not empty"
fi
echo ""

# ============================================================================
# Case statement
# ============================================================================
echo "7. Case Statement"
fruit="apple"
case "$fruit" in
    apple)
        echo "   It's an apple - red or green!"
        ;;
    banana|orange)
        echo "   It's a tropical fruit"
        ;;
    *)
        echo "   Unknown fruit"
        ;;
esac
echo ""

# ============================================================================
# Exit codes
# ============================================================================
echo "8. Exit Codes"
true
echo "   After 'true': \$? = $?"
false
echo "   After 'false': \$? = $?"
echo ""

echo "=== Demo Complete ==="
exit 0

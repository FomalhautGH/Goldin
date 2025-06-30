BUILD=build
SRC=src

$(BUILD)/au: $(BUILD) $(SRC)/main.c $(SRC)/token.c $(SRC)/lexer.c $(SRC)/compiler.c
	clang -ggdb -Wall -Wextra -o ./build/au $(SRC)/main.c $(SRC)/token.c $(SRC)/lexer.c $(SRC)/compiler.c

$(BUILD):
	mkdir -pv $(BUILD)

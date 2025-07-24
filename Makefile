BUILD=build
SRC=src

$(BUILD)/au: $(BUILD) $(SRC)/main.c $(SRC)/token.c $(SRC)/lexer.c $(SRC)/compiler.c $(SRC)/codegen.c
	clang -ggdb -Wall -Wextra -o ./build/au $(SRC)/main.c $(SRC)/token.c $(SRC)/lexer.c $(SRC)/compiler.c $(SRC)/codegen.c

$(BUILD):
	mkdir -pv $(BUILD)

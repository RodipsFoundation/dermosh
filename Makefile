CC = gcc
CFLAGS = -static -s -Wall -Wextra -std=c99 -pedantic -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
TARGET = dermosh
SOURCE = dermosh.c

# Основная цель
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

# Установка
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

# Удаление
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# Очистка
clean:
	rm -f $(TARGET)

# Запуск
run: $(TARGET)
	./$(TARGET)

# Отладочная версия
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

.PHONY: clean install uninstall run debug
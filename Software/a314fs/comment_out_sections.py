with open('a314fs.asm', 'r+b') as f:
    text = f.read()
    text = text.replace('section', ';section')
    f.seek(0, 0)
    f.write(text)

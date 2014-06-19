cocos2d-font-muliple-channel
============================

plugin render font packed in muliple channel for cocos2dx. Implement BMFont and enhance the CCLabelBMFont in cocos2dx. Support 

example:
	LabelBMFont* text = new LabelBMFont();//LabelBMFont::create();

	text->initWithString("test", "bmfont/yahei.fnt",550);

	text->setScale(0.8f);
	text->setString("ÄãºÃ The lvalue-expression evaluates to an l-value. The assignment operator stores the value of the rvalueexpressioninto the l-value and returns an r-value with the type and precision of the lvalue-expression.The lvalue-expression and rvalue-expression must have the same type. All desired type-conversions mustbe specified explicitly via a constructor. Variables that are built-in types, entire structures", true);
	addChild(text);
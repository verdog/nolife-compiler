/**
 * 
 */
package astv3;

import visitor.Visitor;

/**
 * @author carr
 *
 */
public class DeclNode extends ASTNode {

	/**
	 * 
	 */
	public DeclNode() {
		// TODO Auto-generated constructor stub
	}

	/* (non-Javadoc)
	 * @see astv3.ASTNode#accept(visitor.Visitor)
	 */
	@Override
	public void accept(Visitor v) {
		v.visit(this);
	}

}

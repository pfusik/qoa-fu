// Generated automatically with "cito". Do not edit.

/**
 * Least Mean Squares Filter.
 */
class LMS
{
	private final int[] history = new int[4];
	private final int[] weights = new int[4];

	final void init(int i, int h, int w)
	{
		this.history[i] = ((h ^ 128) - 128) << 8;
		this.weights[i] = ((w ^ 128) - 128) << 8;
	}

	final int predict()
	{
		return (this.history[0] * this.weights[0] + this.history[1] * this.weights[1] + this.history[2] * this.weights[2] + this.history[3] * this.weights[3]) >> 13;
	}

	final void update(int sample, int residual)
	{
		int delta = residual >> 4;
		this.weights[0] += this.history[0] < 0 ? -delta : delta;
		this.weights[1] += this.history[1] < 0 ? -delta : delta;
		this.weights[2] += this.history[2] < 0 ? -delta : delta;
		this.weights[3] += this.history[3] < 0 ? -delta : delta;
		this.history[0] = this.history[1];
		this.history[1] = this.history[2];
		this.history[2] = this.history[3];
		this.history[3] = sample;
	}
}
